/*
 * Source of the function to read the temperature and humidity from the HS3001 sensor.
 *
 * Copyright (C) 2022 Renesas Electronics Corp. All rights reserved.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "hs3001.h"

#define HS3001_SLAVE_ADDRESS 0x44
#define I2C_DEVICE_FILE "/dev/i2c-1"

static int measurement_request(int fd) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	int ret = 0;
	uint32_t dummy = 0;
	
	msg[0].addr = HS3001_SLAVE_ADDRESS;
	msg[0].flags = 0;
	msg[0].len = sizeof(uint32_t);
	msg[0].buf = (unsigned char*)&dummy;
	
	packets.msgs = msg;
	packets.nmsgs = 1;

	ret = ioctl(fd, I2C_RDWR, &packets);
	if (ret == -1) {
		fprintf(stderr, "Error: measurement request failed\n");
		return -1;
	}

	return 0;
}

static int data_fetch(int fd, struct hs3001_data *data) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char sensor_data[4];
	int ret = 0;
	float tmp;
	uint16_t humidity;
	int16_t temperature;

	if (data == NULL) {
		fprintf(stderr, "Error: hs3001_data is NULL\n");
		return -1;
	}

	msg[0].addr = HS3001_SLAVE_ADDRESS;
	msg[0].flags = I2C_M_RD;
	msg[0].len = 4;
	msg[0].buf = sensor_data;
	packets.msgs = msg;
	packets.nmsgs = 1;

	ret = ioctl(fd, I2C_RDWR, &packets);
	if (ret == -1) {
		fprintf(stderr, "Error: Data fetch failed\n");
		return -1;
	}

	if (((uint16_t)sensor_data[0] >> 6) == 0) { /* Status Bits is Valid Data */
		tmp = (float)(((uint16_t)sensor_data[0] << 8) | (uint16_t)sensor_data[1]);
		data->humidity = (tmp / 16383.0f) * 100.0f;

		tmp = (float)((((uint16_t)sensor_data[2] << 8) | (uint16_t)sensor_data[3]) >> 2);
		data->temperature = (tmp / 16383.0f) * 165.0f - 40.0f;
	} else {
		fprintf(stderr, "The fetched data was Stale Data: Data that has already been fetched since the last measurement cycle\n");
		return -1;
	}

	return 0;
}

int read_humidity_and_temperature(struct hs3001_data *data) {
	int fd, ret = -1;

	if (data == NULL) {
		fprintf(stderr, "Error: hs3001_data is NULL\n");
		return ret;
	}

	fd = open(I2C_DEVICE_FILE, O_RDWR);
	if (fd == -1) {
		return ret;
	}

	ret = measurement_request(fd);
	if(ret == -1) {
		goto close;
	}

	usleep(HS3001_WAIT_TIME);

	ret = data_fetch(fd, data);
	if(ret == -1) {
		goto close;
	}

close:
	close(fd);

	return ret;
}
