/*
 * ws protocol handler plugin for "graph-update" demonstrating hardware controls
 *
 * Copyright (C) 2019 Renesas Electronics Corp. All rights reserved.
 * This file is based on protocol_lws_minimal.c written in 2010-2019
 * and licensed by CC0 by Andy Green <andy@warmcat.com>
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#define MIN_INTERVAL 250 /* Minimum sensor data reading interval(ms) */

#include <string.h>
#include <time.h>

#include <jansson.h>

/* hardware manipulation */

#include "hs3001.h"
#include "ob1203.h"
#include "pmodled-control.h"

/* one of these created for each message in the ringbuffer */

struct msg {
	void *payload; /* is malloc'd */
	size_t len;
};

/*
 * One of these is created for each client connecting to us.
 *
 * It is ONLY read or written from the lws service thread context.
 */

struct per_session_data__minimal {
	struct per_session_data__minimal *pss_list;
	struct lws *wsi;
	uint32_t tail;
	uint32_t msglen;
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__minimal {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;

	struct per_session_data__minimal *pss_list; /* linked-list of live pss*/
	pthread_t pthread_sensor[1];
	pthread_t pthread_led[1]; /* thread for led control */

	pthread_mutex_t lock_ring; /* serialize access to the ring buffer */
	struct lws_ring *ring; /* {lock_ring} ringbuffer holding unsent content */

	pthread_mutex_t lock_ring_receive; /* serialize access to the ring buffer for receive */
	pthread_cond_t cond_wake_receive; /* wakeup thread for receive */
	struct lws_ring *ring_receive; /* {lock_ring_receive} ringbuffer holding received messages */
	uint32_t tail_receive; /* tail of ring_receive */

	const char *config;
	char finished;
};

/* Sensor data read interval(ms) */

static int read_sensor_data_interval = MIN_INTERVAL;

void
set_interval(int interval)
{
	if(interval >= MIN_INTERVAL) {
		read_sensor_data_interval = interval;
	} else {
		/* The default value for read_sensor_data_interval is used */
	}
}

/*
 * This runs under lws service, "sensor threads" context, and "led threads" context.
 * Access is serialized by vhd->lock_ring or vhd->lock_ring_receive.
 */

static void
__minimal_destroy_message(void *_msg)
{
	struct msg *msg = _msg;

	free(msg->payload);
	msg->payload = NULL;
	msg->len = 0;
}

/*
 * This runs under the "sensor thread" thread context only.
 *
 * We spawn one thread that generate messages with this.
 *
 */

static void *
thread_sensor(void *d)
{
	struct per_vhost_data__minimal *vhd =
			(struct per_vhost_data__minimal *)d;
	struct msg amsg;
	struct hs3001_data hs3001_data;
	struct ob1203_data ob1203_data;
	int len = 512, n, ret = 0;
	int temp_is_active = 1, humm_is_active = 1, light_is_active = 1, proximity_is_active = 1;
	struct timespec start_time;
	struct timespec end_time;
	long tmp_start_time = 0, tmp_end_time = 0, diff_time = 0;

	memset(&hs3001_data, 0, sizeof(hs3001_data));
	memset(&ob1203_data, 0, sizeof(ob1203_data));

	set_ls_status();
	set_ps_measurement_period();
	set_ps_status();

	do {
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		/* don't generate output if nobody connected */
		if (!vhd->pss_list)
			goto wait;

		pthread_mutex_lock(&vhd->lock_ring); /* --------- ring lock { */

		/* only create if space in ringbuffer */
		n = (int)lws_ring_get_count_free_elements(vhd->ring);
		if (!n) {
			lwsl_user("dropping!\n");
			goto wait_unlock;
		}

		amsg.payload = malloc(LWS_PRE + len);
		if (!amsg.payload) {
			lwsl_user("OOM: dropping\n");
			goto wait_unlock;
		}

		ret = read_humidity_and_temperature(&hs3001_data);
		if (ret != 0) {
			lwsl_err("THREAD_SENSOR: ERROR failed to read data from the HS3001 sensor\n");
			temp_is_active = 0;
			humm_is_active = 0;
		}

		ret = read_light(&ob1203_data);
		if (ret != 0) {
			lwsl_err("THREAD_SENSOR: ERROR failed to read light data from the OB1203 sensor\n");
			light_is_active = 0;
		}

		ret = read_proximity(&ob1203_data);
		if (ret != 0) {
			lwsl_err("THREAD_SENSOR: ERROR failed to read proximity data from the OB1203 sensor\n");
			proximity_is_active = 0;
		}

		n = lws_snprintf((char *)amsg.payload + LWS_PRE, len,
			"{\"temp\":{\"value\":\"%2.3f\", \"isActive\":\"%d\"},"
			"\"humm\":{\"value\":\"%2.3f\", \"isActive\":\"%d\"},"
			"\"light\":{\"value\":\"%d\", \"isActive\":\"%d\"},"
			"\"proximity\":{\"value\":\"%d\", \"isActive\":\"%d\"}}",
			hs3001_data.temperature, temp_is_active,
			hs3001_data.humidity, humm_is_active,
			ob1203_data.light, light_is_active,
			ob1203_data.proximity, proximity_is_active);

		amsg.len = n;
		n = lws_ring_insert(vhd->ring, &amsg, 1);

		if (n != 1) {
			__minimal_destroy_message(&amsg);
			lwsl_user("dropping!\n");
		} else
			/*
			 * This will cause a LWS_CALLBACK_EVENT_WAIT_CANCELLED
			 * in the lws service thread context.
			 */
			lws_cancel_service(vhd->context);

wait_unlock:
		pthread_mutex_unlock(&vhd->lock_ring); /* } ring lock ------- */

wait:
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		tmp_start_time = start_time.tv_sec * (LWS_US_PER_SEC * LWS_NS_PER_US) + start_time.tv_nsec;
		tmp_end_time = end_time.tv_sec * (LWS_US_PER_SEC * LWS_NS_PER_US) + end_time.tv_nsec;

		diff_time = (read_sensor_data_interval * LWS_US_PER_MS) - ((tmp_end_time - tmp_start_time) / LWS_NS_PER_US);

		if (diff_time > 0) {
			usleep(diff_time);
		} else {
			/* no sleep */
		}

	} while (!vhd->finished);

	lwsl_notice("thread_spam %p exiting\n", (void *)pthread_self());

	pthread_exit(NULL);

	return NULL;
}

/*
 * This runs under the "led thread" thread context only.
 *
 * We spawn one thread that controll led's GPIO.
 *
 */

static void *
thread_led(void *d)
{
	struct per_vhost_data__minimal *vhd =
			(struct per_vhost_data__minimal *)d;
	const struct msg *pmsg;
	struct msg amsg;
	int len = 128, index = 1, n, ret = 0;

	json_t *root;
	json_t *ledstate;
	json_error_t error;

	do {
		pthread_mutex_lock(&vhd->lock_ring_receive); /* --------- ring lock { */

		for (;;) {
			pmsg = lws_ring_get_element(vhd->ring_receive, &vhd->tail_receive);
			if (pmsg) {
				break;
			} else if (vhd->finished) {
				break;
			}

			pthread_cond_wait(&vhd->cond_wake_receive, &vhd->lock_ring_receive);
		}

		if (vhd->finished) {
			break;
		}

		amsg.len = pmsg->len;
		amsg.payload = malloc(LWS_PRE + len);
		if (!amsg.payload) {
			lwsl_user("THREAD_LED: OOM: dropping\n");
			pthread_mutex_unlock(&vhd->lock_ring_receive); /* } ring lock ------- */
			continue;
		}
		memcpy(amsg.payload + LWS_PRE, pmsg->payload + LWS_PRE, len);

		lws_ring_consume(
			vhd->ring_receive,	/* lws_ring object */
			&vhd->tail_receive,	/* tail of guy doing the consuming */
			NULL,			/* no destination but just delete */
			1			/* number of payload objects being consumed */
		);
		lws_ring_update_oldest_tail(
			vhd->ring_receive,	/* lws_ring object */
			vhd->tail_receive	/* single tail */
		);

		pthread_mutex_unlock(&vhd->lock_ring_receive); /* } ring lock ------- */

		root = json_loadb(((unsigned char *)amsg.payload) + LWS_PRE, amsg.len, 0, &error);
		free(amsg.payload);

		if (!root) {
			lwsl_err("THREAD_LED: ERROR json parse error on line %d: %s\n", error.line, error.text);
			continue;
		}
		if (!json_is_object(root)) {
			lwsl_err("THREAD_LED: ERROR json top is not an object\n");
			json_decref(root);
			continue;
		}

		ledstate = json_object_get(root, "led");
		if (!json_is_string(ledstate)) {
			lwsl_err("THREAD_LED: ERROR json has no key \"led\" with string value\n");
			json_decref(root);
			continue;
		}
		lwsl_user("THREAD_LED: led: %s\n", json_string_value(ledstate));
		if (strcmp(json_string_value(ledstate), "on") == 0) {
			ret = led_on();
			if(ret != 0) {
				lwsl_err("%s\n", strerror(ret));
			}
		} else if (strcmp(json_string_value(ledstate), "off") == 0) {
			ret = led_off();
			if(ret != 0) {
				lwsl_err("%s\n", strerror(ret));
			}
		} else {
			lwsl_err("THREAD_LED: ERROR unknown value \"%s\" to the key \"led\"\n", json_string_value(ledstate));
		}

		json_decref(ledstate);
		json_decref(root);

	} while (!vhd->finished);

	lwsl_notice("thread_led %p exiting\n", (void *)pthread_self());

	pthread_exit(NULL);

	return NULL;
}

/* this runs under the lws service thread context only */

static int
callback_minimal(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
	struct per_session_data__minimal *pss =
			(struct per_session_data__minimal *)user;
	struct per_vhost_data__minimal *vhd =
			(struct per_vhost_data__minimal *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	const struct lws_protocol_vhost_options *pvo;
	const struct msg *pmsg;
	struct msg amsg;
	void *retval;
	int n, m, r = 0;

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		/* create our per-vhost struct */
		vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__minimal));
		if (!vhd)
			return 1;

		if (led_prepare()) {
			lwsl_err("%s: Can't export pmodled's GPIO\n", __func__);
			return 1;
		}

		pthread_mutex_init(&vhd->lock_ring, NULL);

		pthread_mutex_init(&vhd->lock_ring_receive, NULL);

		/* recover the pointer to the globals struct */
		pvo = lws_pvo_search(
			(const struct lws_protocol_vhost_options *)in,
			"config");
		if (!pvo || !pvo->value) {
			lwsl_err("%s: Can't find \"config\" pvo\n", __func__);
			return 1;
		}
		vhd->config = pvo->value;

		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);

		vhd->ring = lws_ring_create(sizeof(struct msg), 8,
					    __minimal_destroy_message);
		if (!vhd->ring) {
			lwsl_err("%s: failed to create ring\n", __func__);
			return 1;
		}

		vhd->ring_receive = lws_ring_create(sizeof(struct msg), 8,
					    __minimal_destroy_message);
		if (!vhd->ring_receive) {
			lwsl_err("%s: failed to create ring\n", __func__);
			return 1;
		}

		pthread_cond_init(&vhd->cond_wake_receive, NULL);

		/* start the content-creating threads */

		for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pthread_sensor); n++)
			if (pthread_create(&vhd->pthread_sensor[n], NULL,
					   thread_sensor, vhd)) {
				lwsl_err("thread creation failed\n");
				r = 1;
				goto init_fail;
			}

		for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pthread_led); n++)
			if (pthread_create(&vhd->pthread_led[n], NULL,
					   thread_led, vhd)) {
				lwsl_err("thread creation failed\n");
				r = 1;
				goto init_fail;
			}
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
init_fail:
		vhd->finished = 1;
		pthread_cond_signal(&vhd->cond_wake_receive); /* wake up pthread_led */
		for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pthread_sensor); n++)
			if (vhd->pthread_sensor[n])
				pthread_join(vhd->pthread_sensor[n], &retval);

		for (n = 0; n < (int)LWS_ARRAY_SIZE(vhd->pthread_led); n++)
			if (vhd->pthread_led[n])
				pthread_join(vhd->pthread_led[n], &retval);

		if (vhd->ring)
			lws_ring_destroy(vhd->ring);

		if (vhd->ring_receive)
			lws_ring_destroy(vhd->ring_receive);

		pthread_mutex_destroy(&vhd->lock_ring);
		pthread_mutex_destroy(&vhd->lock_ring_receive);
		pthread_cond_destroy(&vhd->cond_wake_receive);

		break;

	case LWS_CALLBACK_ESTABLISHED:
		/* add ourselves to the list of live pss held in the vhd */
		lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
		pss->tail = lws_ring_get_oldest_tail(vhd->ring);
		pss->wsi = wsi;
		break;

	case LWS_CALLBACK_CLOSED:
		/* remove our closing pss from the list of live pss */
		lws_ll_fwd_remove(struct per_session_data__minimal, pss_list,
				  pss, vhd->pss_list);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		pthread_mutex_lock(&vhd->lock_ring); /* --------- ring lock { */

		pmsg = lws_ring_get_element(vhd->ring, &pss->tail);
		if (!pmsg) {
			pthread_mutex_unlock(&vhd->lock_ring); /* } ring lock ------- */
			break;
		}

		/* notice we allowed for LWS_PRE in the payload already */
		m = lws_write(wsi, ((unsigned char *)pmsg->payload) + LWS_PRE,
			      pmsg->len, LWS_WRITE_TEXT);
		if (m < (int)pmsg->len) {
			pthread_mutex_unlock(&vhd->lock_ring); /* } ring lock ------- */
			lwsl_err("ERROR %d writing to ws socket\n", m);
			return -1;
		}

		lws_ring_consume_and_update_oldest_tail(
			vhd->ring,	/* lws_ring object */
			struct per_session_data__minimal, /* type of objects with tails */
			&pss->tail,	/* tail of guy doing the consuming */
			1,		/* number of payload objects being consumed */
			vhd->pss_list,	/* head of list of objects with tails */
			tail,		/* member name of tail in objects with tails */
			pss_list	/* member name of next object in objects with tails */
		);

		/* more to do? */
		if (lws_ring_get_element(vhd->ring, &pss->tail))
			/* come back as soon as we can write more */
			lws_callback_on_writable(pss->wsi);

		pthread_mutex_unlock(&vhd->lock_ring); /* } ring lock ------- */
		break;

	case LWS_CALLBACK_RECEIVE:
		lwsl_user("LWS_CALLBACK_RECEIVE: %4d (rpp %5d, first %d, "
			"last %d, bin %d, len %d)\n",
			(int)len, (int)lws_remaining_packet_payload(wsi),
			lws_is_first_fragment(wsi),
			lws_is_final_fragment(wsi),
			lws_frame_is_binary(wsi), (int)len);

		if (len) {
			lwsl_user("LWS_CALLBACK_RECEIVE: %.*s\n", (int)len, (const char *)in);
		}

		amsg.len = len;
		/* notice we over-allocate by LWS_PRE */
		amsg.payload = malloc(LWS_PRE + len);
		if (!amsg.payload) {
			lwsl_user("OOM: dropping\n");
			break;
		}

		memcpy((char *)amsg.payload + LWS_PRE, in, len);

		pthread_mutex_lock(&vhd->lock_ring_receive); /* --------- ring lock { */

		n = (int)lws_ring_get_count_free_elements(vhd->ring_receive);
		if (!n) {
			pthread_mutex_unlock(&vhd->lock_ring_receive); /* } ring lock ------- */
			lwsl_user("dropping!\n");
			free(amsg.payload);
			break;
		}

		if (!lws_ring_insert(vhd->ring_receive, &amsg, 1)) {
			__minimal_destroy_message(&amsg);
			pthread_mutex_unlock(&vhd->lock_ring_receive); /* } ring lock ------- */
			lwsl_user("dropping 2!\n");
			free(amsg.payload);
			break;
		}

		pthread_cond_signal(&vhd->cond_wake_receive); /* wake up pthread_led */
		pthread_mutex_unlock(&vhd->lock_ring_receive); /* } ring lock ------- */

		break;

	case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		if (!vhd)
			break;
		/*
		 * When the sensor threads add a message to the ringbuffer,
		 * they create this event in the lws service thread context
		 * using lws_cancel_service().
		 *
		 * We respond by scheduling a writable callback for all
		 * connected clients.
		 */
		lws_start_foreach_llp(struct per_session_data__minimal **,
				      ppss, vhd->pss_list) {
			lws_callback_on_writable((*ppss)->wsi);
		} lws_end_foreach_llp(ppss, pss_list);
		break;

	default:
		break;
	}

	return r;
}

#define LWS_PLUGIN_PROTOCOL_MINIMAL \
	{ \
		"graph-update", \
		callback_minimal, \
		sizeof(struct per_session_data__minimal), \
		128, \
		0, NULL, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)

/* boilerplate needed if we are built as a dynamic plugin */

static const struct lws_protocols protocols[] = {
	LWS_PLUGIN_PROTOCOL_MINIMAL
};

LWS_EXTERN LWS_VISIBLE int
init_protocol_minimal(struct lws_context *context,
		      struct lws_plugin_capability *c)
{
	if (c->api_magic != LWS_PLUGIN_API_MAGIC) {
		lwsl_err("Plugin API %d, library API %d", LWS_PLUGIN_API_MAGIC,
			 c->api_magic);
		return 1;
	}

	c->protocols = protocols;
	c->count_protocols = LWS_ARRAY_SIZE(protocols);
	c->extensions = NULL;
	c->count_extensions = 0;

	return 0;
}

LWS_EXTERN LWS_VISIBLE int
destroy_protocol_minimal(struct lws_context *context)
{
	return 0;
}
#endif
