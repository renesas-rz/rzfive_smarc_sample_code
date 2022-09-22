/*
 * Source of the function to read the proximity and light from the OB1203 sensor.
 *
 * Copyright (C) 2022 Renesas Electronics Corp. All rights reserved.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "ob1203.h"

#define OB1203_SLAVE_ADDRESS 0x53
#define I2C_DEVICE_FILE "/dev/i2c-1"

static int read_i2c_data(int fd, unsigned char register_address, unsigned char *data, int size) {
	struct i2c_msg msg[2];
	struct i2c_rdwr_ioctl_data packets;
	int ret;

	msg[0].addr = OB1203_SLAVE_ADDRESS;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &register_address;

	msg[1].addr = OB1203_SLAVE_ADDRESS;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = data;

	packets.msgs = msg;
	packets.nmsgs = 2;
	ret = ioctl(fd, I2C_RDWR, &packets);

	return ret;
}

static int write_i2c_data(int fd, unsigned char register_address, unsigned char data, int size) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char buf[size + 1]; /* Allocate size byte of data to be written + 1 byte of register address */
	int ret;

	buf[0] = register_address;
	buf[1] = data;

	msg[0].addr = OB1203_SLAVE_ADDRESS;
	msg[0].flags = 0;
	msg[0].len = sizeof(buf);
	msg[0].buf = buf;

	packets.msgs = msg;
	packets.nmsgs = 1;
	ret = ioctl(fd, I2C_RDWR, &packets);

	return ret;
}

int set_ls_status() {
	int size = 1;
	int fd, ret = 0;
	unsigned char register_address = 0x15, data = 0x03;

	fd = open(I2C_DEVICE_FILE, O_RDWR);
	if (fd == -1) {
		return -1;
	}

	/* set Light sensor mode: CS Mode */
	/* set Light sensor enable: Light sensor active */
	ret = write_i2c_data(fd, register_address, data, size);

	if(ret == -1){
		fprintf(stderr, "Error: light sensor activation failed\n");
		return -1;
	}

	close(fd);
	return 0;
}

int set_ps_status() {
	int fd, ret = 0, size = 1;
	unsigned char register_address = 0x16, data = 0x01;

	fd = open(I2C_DEVICE_FILE, O_RDWR);
	if (fd == -1) {
		return -1;
	}

	/* set PPG proximity mode: PS Mode(default) */
	/* set PPG or proximity sensor enable: PPG/PS active */
	ret = write_i2c_data(fd, register_address, data, size);

	if(ret == -1){
		fprintf(stderr, "Error: proximity sensor activation failed\n");
		return -1;
	}

	close(fd);
	return 0;
}

int set_ps_measurement_period() {
	int fd, ret = 0, size = 1;
	unsigned char register_address = 0x1A, data = 0x14;

	fd = open(I2C_DEVICE_FILE, O_RDWR);
	if (fd == -1) {
		return -1;
	}

	/* set PS_measurement_period: 50ms */
	ret = write_i2c_data(fd, register_address, data, size);

	if(ret == -1){
		fprintf(stderr, "Error: Failed to update the ps measurement period\n");
		return -1;
	}

	close(fd);
	return 0;
}

static int read_ls_data_status(int fd) {
	int size = 1, ret;
	unsigned char ls_data_status = 0;
	unsigned char register_address = 0x00;

	ret = read_i2c_data(fd, register_address, &ls_data_status, size);
	if(ret == -1){
		fprintf(stderr, "Error: Failed to read LS_data_status\n");
		return -1;
	}

	return ls_data_status & 0x01;
}

static int read_ps_data_status(int fd) {
	int size = 1, ret;
	unsigned char ps_data_status = 0;
	unsigned char register_address = 0x01;

	ret = read_i2c_data(fd, register_address, &ps_data_status, size);
	if(ret == -1){
		fprintf(stderr, "Error: Failed to read PS_data_status\n");
		return -1;
	}

	return ps_data_status & 0x01;
}

static int read_ls_green_data(int fd) {
	int size = 1, ret;
	unsigned char color_green = 0;
	unsigned char register_address = 0x07;

	ret = read_i2c_data(fd, register_address, &color_green, size);
	if(ret == -1){
		fprintf(stderr, "Error: Failed to read LS_GREEN_DATA\n");
		return -1;
	}
	return color_green;
}

static int read_ls_blue_data(int fd) {
	int size = 1, ret;
	unsigned char color_blue = 0;
	unsigned char register_address = 0x0A;

	ret = read_i2c_data(fd, register_address, &color_blue, size);
	if(ret == -1){
		fprintf(stderr, "Error: Failed to read LS_BLUE_DATA\n");
		return -1;
	}

	return color_blue;
}

static int read_ls_red_data(int fd) {
	int size = 1, ret;
	unsigned char color_red = 0;
	unsigned char register_address = 0x0D;

	ret = read_i2c_data(fd, register_address, &color_red, size);
	if(ret == -1){
		fprintf(stderr, "Error: Failed to read LS_RED_DATA\n");
		return -1;
	}

	return color_red;
}

static int read_ps_data(int fd) {
	int size = 2, ret;
	unsigned char data[size];
	unsigned char register_address = 0x02;
	unsigned int proximity;

	ret = read_i2c_data(fd, register_address, data, size);
	if(ret == -1){
		fprintf(stderr, "Error: Failed to read PS_DATA\n");
		return -1;
	}

	proximity = (data[1] << 8) | data[0];

	return proximity;
}

static int calc_light(int color_green, int color_blue, int color_red) {
	int gain = 3, res = 18, c = 1;
	int gain_scale = 0, res_scale = 0;
	int light = 0;

	res_scale = pow(2, 20 - res);
	gain_scale = 6 / gain;
	light = gain_scale * res_scale * ((color_red + color_green + color_blue) * c);

	return light;
}

int read_light(struct ob1203_data *data) {
	int fd, ret = 0, color_green = 0, color_blue = 0, color_red = 0;
	unsigned char ls_data_status;

	fd = open(I2C_DEVICE_FILE, O_RDWR);
	if (fd == -1) {
		return -1;
	}

	if (data == NULL) {
		fprintf(stderr, "Error: ob1203_data is NULL\n");
		ret = -1;
		goto close;
	}

	ls_data_status = read_ls_data_status(fd);

	if (ls_data_status == 0) {
		printf("The LS data is an old data, already read\n");
		usleep(OB1203_LS_WAIT_TIME);
	}

	color_green = read_ls_green_data(fd);
	if(color_green == -1) {
		ret = -1;
		goto close;
	}

	color_blue = read_ls_blue_data(fd);
	if(color_blue == -1) {
		ret = -1;
		goto close;
	}

	color_red = read_ls_red_data(fd);
	if(color_red == -1) {
		ret = -1;
		goto close;
	}

	data->color_green = color_green;
	data->color_blue = color_blue;
	data->color_red = color_red;
	data->light = calc_light(data->color_red, data->color_green, data->color_blue);

close:
	close(fd);
	return ret;
}

int read_proximity(struct ob1203_data * data) {
	int fd ,ret = 0, proximity = 0;
	unsigned char ps_data_status;

	fd = open(I2C_DEVICE_FILE, O_RDWR);
	if (fd == -1) {
		return -1;
	}

	if (data == NULL) {
		fprintf(stderr, "Error: ob1203_data is NULL\n");
		ret = -1;
		goto close;
	}

	ps_data_status = read_ps_data_status(fd);

	if (ps_data_status == 0) {
		printf("The PS data is an old data, already read\n");
		usleep(OB1203_PS_WAIT_TIME);
	}

	proximity = read_ps_data(fd);
	if(proximity == -1) {
		ret = -1;
		goto close;
	}
	data->proximity = proximity;

close:
	close(fd);
	return ret;
}