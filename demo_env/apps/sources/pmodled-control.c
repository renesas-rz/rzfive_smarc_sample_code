/*
 * Led control functions header
 *
 * Copyright (C) 2022 Renesas Electronics Corp. All rights reserved.
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h> /* for snprintf */
#include <string.h> /* for strlen */
#include <errno.h> /* for errno */

#include "pmodled-control.h"

#define GPIO_LD0 268
#define GPIO_LD1 128
#define GPIO_LD2 131
#define GPIO_LD3 132

#define PIN_GPIO_LD0 "P18_4"
#define PIN_GPIO_LD1 "P1_0"
#define PIN_GPIO_LD2 "P1_3"
#define PIN_GPIO_LD3 "P1_4"

#define LED_ON "high"
#define LED_OFF "low"

int gpio_sysfs_export(int gpio, char* pin) {
	int fd;
	char path[64];
	char buf[16];
	int count, ret = 0;

	memset(path, 0, sizeof(path));
	memset(buf, 0, sizeof(buf));

	/* test if the target GPIO sysfs is alreayd exported */
	snprintf(path, sizeof(path)-1, "/sys/class/gpio/%s/direction", pin);
	fd = open(path, O_RDONLY);
	if (fd != -1) { /* when the target GPIO sysfs is exported */
		close(fd);
		return 0;
	}

	snprintf(buf, sizeof(buf)-1, "%d", gpio);
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd == -1) { /* open error */
		fprintf(stderr, "%s\n", strerror(errno));
		ret =  -errno;
		goto close;
	}
	count = write(fd, buf, strlen(buf));
	if (count == -1) { /* write error */
		fprintf(stderr, "%s\n", strerror(errno));
		ret = -errno;
		goto close;
	}

close:
	close(fd);
	return ret;
}

int gpio_sysfs_direction(char* pin, char* direction) {
	int fd;
	char path[64];
	int count, ret = 0;

	memset(path, 0, sizeof(path));

	snprintf(path, sizeof(path)-1, "/sys/class/gpio/%s/direction", pin);
	fd = open(path, O_WRONLY | O_NONBLOCK);
	if (fd == -1) { /* open error */
		fprintf(stderr, "%s\n", strerror(errno));
		ret = -errno;
		goto close;
	}
	count = write(fd, direction, strlen(direction));
	if (count == -1) { /* write error */
		fprintf(stderr, "%s\n", strerror(errno));
		ret = -errno;
		goto close;
	}

close:
	close(fd);
	return ret;
}

int led_prepare(void) {
	int result = 0;

	result = gpio_sysfs_export(GPIO_LD0, PIN_GPIO_LD0);
	if (result) {
		return result;
	}

	result = gpio_sysfs_export(GPIO_LD1, PIN_GPIO_LD1);
	if (result) {
		return result;
	}

	result = gpio_sysfs_export(GPIO_LD2, PIN_GPIO_LD2);
	if (result) {
		return result;
	}

	result = gpio_sysfs_export(GPIO_LD3, PIN_GPIO_LD3);
	if (result) {
		return result;
	}

	usleep(1000000); /* wait for udev handling of GPIO sysfs */

	result = gpio_sysfs_direction(PIN_GPIO_LD0, LED_OFF);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD1, LED_OFF);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD2, LED_OFF);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD3, LED_OFF);
	if (result) {
		return result;
	}

	return 0;
}

int led_on(void) {
	int result = 0;

	result = gpio_sysfs_direction(PIN_GPIO_LD0, LED_ON);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD1, LED_ON);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD2, LED_ON);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD3, LED_ON);
	if (result) {
		return result;
	}

	return 0;
}

int led_off(void) {
	int result = 0;

	result = gpio_sysfs_direction(PIN_GPIO_LD0, LED_OFF);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD1, LED_OFF);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD2, LED_OFF);
	if (result) {
		return result;
	}

	result = gpio_sysfs_direction(PIN_GPIO_LD3, LED_OFF);
	if (result) {
		return result;
	}

	return 0;
}

