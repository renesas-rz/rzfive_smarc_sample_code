/*
 * lws-minimal-ws-server
 *
 * Copyright (C) 2019 Renesas Electronics Corp. All rights reserved.
 * This file is based on minimal-ws-server.c written in 2010-2019
 * and licensed by CC0 by Andy Green <andy@warmcat.com>
 *
 * This demonstrates a minimal ws server that can cooperate with
 * other threads for interacting with hardware. Two other threads are started. One of them fills
 * a ringbuffer with the sensor data. Another of them read a ringbuffer and controls the fan GPIO.
 *
 * The actual work and thread spawning etc are done in the protocol
 * implementation in protocol_graph_update.c.
 *
 * To keep it simple, it serves stuff in the subdirectory "./" of
 * the directory it was started in.
 * You can change that by changing mount.origin.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#define LWS_PLUGIN_STATIC
#include "protocol_graph_update.c"

static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0 },
	LWS_PLUGIN_PROTOCOL_MINIMAL,
	{ NULL, NULL, 0, 0 } /* terminator */
};

static int interrupted;

static const struct lws_http_mount mount = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./", /* serve from dir */
	/* .def */			"index.html",	/* default filename */
	/* .protocol */			NULL,
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .origin_protocol */		LWSMPRO_FILE,	/* files in a dir */
	/* .mountpoint_len */		1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

/*
 * This demonstrates how to pass a pointer into a specific protocol handler
 * running on a specific vhost.  In this case, it's our default vhost and
 * we pass the pvo named "config" with the value a const char * "myconfig".
 *
 * This is the preferred way to pass configuration into a specific vhost +
 * protocol instance.
 */

static const struct lws_protocol_vhost_options pvo_ops = {
	NULL,
	NULL,
	"config",		/* pvo name */
	(void *)"myconfig"	/* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
	NULL,		/* "next" pvo linked-list */
	&pvo_ops,	/* "child" pvo linked-list */
	"graph-update",	/* protocol name we belong to on this vhost */
	""		/* ignored */
};

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

	int interval = 0;

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	/* argv[1]: interval(ms) */
	if(argv[1] == NULL) {
		/* The default value for read_sensor_data_interval is used */
	} else {
		interval = atoi(argv[1]);
		set_interval(interval);
	}

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal ws server + threads | visit http://localhost:3000\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 3000;
	info.mounts = &mount;
	info.protocols = protocols;
	info.pvo = &pvo; /* per-vhost options */
	info.options =
		LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* start the threads that create content */

	while (!interrupted)
		if (lws_service(context, 0))
			interrupted = 1;

	lws_context_destroy(context);

	return 0;
}
