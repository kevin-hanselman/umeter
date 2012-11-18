#include "umeter_datalog.h"

#include <string.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include "fat.h"
#include "fat_config.h"
#include "partition.h"
#include "sd_raw.h"
#include "sd_raw_config.h"

static struct fat_fs_struct* fs;	// filesystem object
static struct fat_dir_struct* dd;	// current directory object

static uint8_t find_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name, struct fat_dir_entry_struct* dir_entry);
static struct fat_file_struct* open_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name);

void SDCardManager_Init(void)
{
	while(!sd_raw_init())
		printf_P(PSTR("MMC/SD initialization failed\r\n"));

	/* open first partition */
        struct partition_struct* partition = partition_open(sd_raw_read,
                                                            sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
                                                            sd_raw_write,
                                                            sd_raw_write_interval,
#else
                                                            0,
                                                            0,
#endif
                                                            0
                                                           );

        if(!partition)
        {
            /* If the partition did not open, assume the storage device
             * is a "superfloppy", i.e. has no MBR.
             */
            partition = partition_open(sd_raw_read,
                                       sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
                                       sd_raw_write,
                                       sd_raw_write_interval,
#else
                                       0,
                                       0,
#endif
                                       -1
                                      );
            if(!partition)
            {
#if DEBUG
                printf_P(PSTR("opening partition failed\r\n"));
#endif
                return;
            }
        }

        /* open file system */
        fs = fat_open(partition);
        if(!fs)
        {
#if DEBUG
            printf_P(PSTR("opening filesystem failed\r\n"));
#endif
            return;
        }

        /* open root directory */
        struct fat_dir_entry_struct directory;
        fat_get_dir_entry_of_path(fs, "/", &directory);

        dd = fat_open_dir(fs, &directory);
        if(!dd)
        {
#if DEBUG
            printf_P(PSTR("opening root directory failed\r\n"));
#endif
            return;
        }

}


void UMeter_Init(void)
{
  // create file
  struct fat_dir_entry_struct file_entry;
  if(!fat_create_file(dd, "umeter.txt", &file_entry))
  {
      printf_P(PSTR("error creating file\r\n"));
  }

}


void UMeter_Write(void)
{
  static int x=1;
  unsigned char buff[8];

  printf_P(PSTR("writing...\r\n"));

  // search file in current directory and open it
  struct fat_file_struct* fd = open_file_in_dir(fs, dd, "umeter.txt");
  if(!fd)
  {
      printf_P(PSTR("error opening file\r\n"));
      return;
  }

  while(fat_read_file(fd, buff, sizeof(buff)) == sizeof(buff));

  int n = sprintf(buff, "line %d\n", x++);

  // write text to file
  if(fat_write_file(fd, buff, n) != n)
  {
      printf_P(PSTR("error writing to file\r\n"));
  }

  fat_close_file(fd);
}

uint8_t find_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name, struct fat_dir_entry_struct* dir_entry)
{
    while(fat_read_dir(dd, dir_entry))
    {
        if(strcmp(dir_entry->long_name, name) == 0)
        {
            fat_reset_dir(dd);
            return 1;
        }
    }

    return 0;
}

struct fat_file_struct* open_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name)
{
    struct fat_dir_entry_struct file_entry;
    if(!find_file_in_dir(fs, dd, name, &file_entry))
        return 0;

    return fat_open_file(fs, &file_entry);
}