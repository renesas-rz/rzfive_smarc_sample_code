 /*
 * Header of the function to read the temperature and humidity from the HS3001 sensor.
 *
 * Copyright (C) 2022 Renesas Electronics Corp. All rights reserved.
 */

#ifndef _HS3001_H_

#define HS3001_WAIT_TIME 50000

/* retain the value of the hs3001 sensor */

struct hs3001_data {
	float humidity;
	float temperature;
};

int read_humidity_and_temperature(struct hs3001_data *data);

#endif /* _HS3001_H_ */
