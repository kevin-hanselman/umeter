#include "umeter_ini.h"

#include <string.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "ini.h"

static umeter_config umeter;

static int ini_handler(void* user,
					   const char* section,
					   const char* name,
					   const char* value)
{
	umeter_config* pconfig = (umeter_config*)user;
#if INI_DEBUG	
	printf_P(PSTR("ini_handler: section=%s, name=%s, value=%s\r\n"), section, name, value);
#endif
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("", "sampling_interval")) {
        pconfig->sampling_interval = atoi(value);
    } else if (MATCH("", "username")) {
        pconfig->username = strdup(value);
    } else {
		printf_P(PSTR("ini_handler: unknown section/name\r\n"));
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

umeter_config const * get_umeter_ini(struct fat_fs_struct* fs, struct fat_dir_struct* dir)
{
	static int populated = 0;
	int err;
	if(!populated) {
		err = ini_parse("umeter.ini", ini_handler, &umeter, fs, dir);
		if (err < 0) {
			printf_P(PSTR("Can't load/parse 'umeter.ini'\r\n"));
			return 0;
		}
		if (err > 0) {
			printf_P(PSTR("Bad config file (first error on line %d)\r\n"), err);
			return 0;
		}
		populated = 1;
	}
	printf_P(PSTR("Loaded 'umeter.ini': sampling_interval=%d, username=%s\r\n"), umeter.sampling_interval, umeter.username);
    return &umeter;
}