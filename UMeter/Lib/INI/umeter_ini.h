#ifndef __UMETER_INI_H__
#define __UMETER_INI_H__

#include "Lib/FatSD/fat.h"

typedef struct
{
	int sampling_interval;
	const char* username;
} umeter_config;

static int ini_handler(void* user,
					   const char* section,
					   const char* name,
					   const char* value);

umeter_config const * get_umeter_ini(struct fat_fs_struct* fs, struct fat_dir_struct* dir);

#endif