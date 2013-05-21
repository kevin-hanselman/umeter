#ifndef __UMETER_INI_H__
#define __UMETER_INI_H__

#include "lib/FatSD/fat.h"
#include <limits.h>

#define SAMPLING_MAX INT_MAX
#define SAMPLING_MIN 100

typedef struct
{
	// if the sensor measurement should be written to the SD card
	uint8_t enabled;

	// if the sensor measurement should be a raw voltage
	uint8_t raw_output;

	// only will be used if 'raw_output' is false
	char* units;		// string representing the units converted to
	float offset;		// calibration linear offset value
	float slope;		// calibration scaler/slope value
} sensor;

typedef struct
{
	unsigned int sampling_interval;
	sensor sensors[4];
} umeter_config;

enum
{
	sensor1=0, sensor2, sensor3, sensor4
} sensor_indeces;

static int ini_handler(void* user,
					   const char* section,
					   const char* name,
					   const char* value);

umeter_config const* get_umeter_ini(struct fat_fs_struct* fs, struct fat_dir_struct* dir);

void print_config(void);

#endif
