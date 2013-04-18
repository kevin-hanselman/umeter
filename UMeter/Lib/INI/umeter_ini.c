#include "umeter_ini.h"

#include <string.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "ini.h"
#include "Lib/Inputs/umeter_adc.h"

static umeter_config umeter;

static int ini_handler(void* user,
					   const char* section,
					   const char* name,
					   const char* value)
{
	uint8_t InvalidValue = 0;
	unsigned int x;
	float y;
	int8_t sensor_idx = -1;
	umeter_config* pconfig = (umeter_config*)user;
#if INI_DEBUG	
	printf_P(PSTR("ini_handler: section=%s, name=%s, value=%s\r\n"), section, name, value);
#endif
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("UMeter", "sampling_interval")) {
		x = atoi(value);
		//printf("x=%d\r\n", x);
		if(x >= SAMPLING_MIN && x <= SAMPLING_MAX) {
			pconfig->sampling_interval = x;
		}
		else {
			InvalidValue = 1;
		}
    } else if (strcmp(section, "Sensor 1") == 0) {
		sensor_idx = sensor1;
    } else if (strcmp(section, "Sensor 2") == 0) {
		sensor_idx = sensor2;
	} else if (strcmp(section, "Sensor 3") == 0) {
		sensor_idx = sensor3;
    } else if (strcmp(section, "Sensor 4") == 0) {
		sensor_idx = sensor4;
	}
	if(sensor_idx >= 0) { // sensor index valid
		if(strcmp(name,"enabled") == 0) {
			pconfig->sensors[sensor_idx].enabled = atoi(value);
		} else if(strcmp(name,"raw_output") == 0) {
			pconfig->sensors[sensor_idx].raw_output = atoi(value);
		} else if(strcmp(name,"units") == 0) {
			pconfig->sensors[sensor_idx].units = strdup(value);
		} else if(strcmp(name,"offset") == 0) {
			pconfig->sensors[sensor_idx].offset = atof(value);
		} else if(strcmp(name,"slope") == 0) {
			y = atof(value);
			if(y) { // avoid divide by zero
				pconfig->sensors[sensor_idx].slope = y;
			}
			else {
				InvalidValue = 1;
			}
		}
	}
// 	if(0) {
// 		printf_P(PSTR("ini_handler: unknown section/name\r\n"));
//         return 0;  /* unknown section/name, error */
//     }
    if(InvalidValue) {
		printf_P(PSTR("ini_handler: invalid value for '%s': '%s'. Using default value.\r\n"), section, name);
	}
    return 1;
}

const umeter_config const* get_umeter_ini(struct fat_fs_struct* fs, struct fat_dir_struct* dir)
{
	static int populated = 0;
	int err;
	if(!populated) {
		const sensor sensor_defaults = {
			1,		// enabled
			1,		// raw_output
			"n/a",	// units, won't be used
			0.0, 	// offset, won't be used
			1.0		// slope, won't be used
		};

		const umeter_config umeter_defaults = {
			1000, // sampling_interval
			{sensor_defaults, sensor_defaults, sensor_defaults, sensor_defaults}
		};
		umeter = umeter_defaults;
		err = ini_parse("umeter.ini", ini_handler, &umeter, fs, dir);
		if (err < 0) {
			printf_P(PSTR("Can't load/parse 'umeter.ini'\r\n"));
			return 0;
		}
		if (err > 0) {
			printf_P(PSTR("Bad config file (first error on line %d)\r\n"), err);
			return 0;
		}
		printf_P(PSTR("Loaded 'umeter.ini': \r\n"));
		print_config();
		populated = 1;
	}
    return &umeter;
}

void print_config(void)
{
	int i;
	printf_P(PSTR("UMETER CONFIG\r\nsampling_interval=%d\r\n"), umeter.sampling_interval);
	for(i=0; i<4; i++) {
		sensor s = umeter.sensors[i];
		char offset[8];
		float2str(s.offset, offset);
		char slope[8];
		float2str(s.slope, slope);
		printf_P(PSTR("Sensor %d: enabled=%d, raw_output=%d, units=%s, offset=%s, slope=%s\r\n"),
				i+1, s.enabled, s.raw_output, s.units, offset, slope);
	}
}