 /*
 * Header of the function to read the proximity and light from the OB1203 sensor.
 *
 * Copyright (C) 2022 Renesas Electronics Corp. All rights reserved.
 */

#ifndef _OB1203_H_

#define OB1203_LS_MEASUREMRNT_TIME 100000
#define OB1203_LS_WAIT_TIME OB1203_LS_MEASUREMRNT_TIME
#define OB1203_PS_MEASUREMRNT_TIME 50000
#define OB1203_PS_WAIT_TIME OB1203_PS_MEASUREMRNT_TIME

/* retain the value of the ob1203 sensor */

struct ob1203_data {
	int color_green;
	int color_blue;
	int color_red;
	int light;
	int proximity;
};

int set_ls_status();
int set_ps_status();
int set_ps_measurement_period();
int read_light(struct ob1203_data *data);
int read_proximity(struct ob1203_data *data);

#endif /* _OB1203_H_ */
