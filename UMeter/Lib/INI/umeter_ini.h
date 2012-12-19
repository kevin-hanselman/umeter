#ifndef __UMETER_INI_H__
#define __UMETER_INI_H__

#include "Lib/FatSD/fat.h"
#include <limits.h>

#define SAMPLING_MAX INT_MAX
#define SAMPLING_MIN 100

typedef struct
{
	unsigned int sampling_interval;
	const char* username;
} umeter_config;

//extern const umeter_config umeter_defaults;

static int ini_handler(void* user,
					   const char* section,
					   const char* name,
					   const char* value);

umeter_config const* get_umeter_ini(struct fat_fs_struct* fs, struct fat_dir_struct* dir);

#endif