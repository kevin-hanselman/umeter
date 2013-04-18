/*
			LUFA Library
	Copyright (C) Dean Camera, 2009.

dean [at] fourwalledcubicle [dot] com
	www.fourwalledcubicle.com
*/

/*
Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of the author not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

The author disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall the author be liable for any
special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
*/

/** \file
*
*  Functions to manage the physical dataflash media, including reading and writing of
*  blocks of data. These functions are called by the SCSI layer when data must be stored
*  or retrieved to/from the physical storage media. If a different media is used (such
*  as a SD card or EEPROM), functions similar to these will need to be generated.
*/

#define  INCLUDE_FROM_SDCARDMANAGER_C
#include "SDCardManager.h"

#include <string.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include "fat.h"
#include "fat_config.h"
#include "partition.h"
#include "sd_raw.h"
#include "sd_raw_config.h"

#include "Lib/INI/umeter_ini.h"
#include "Lib/Inputs/umeter_adc.h"

#define DEBUG 1
// teensy
//#define LED_ON()	PORTD |= (1<<PD6)
//#define LED_OFF()	PORTD &= ~(1<<PD6)

// pcb
#define LED_ON()	PORTB &= ~(1<<PB6)
#define LED_OFF()	PORTB |= (1<<PB6)

static struct sd_raw_info disk_info;
static uint32_t CachedTotalBlocks = 0;
static uint8_t Buffer[16];

static struct fat_fs_struct* fs;	// filesystem object
static struct fat_dir_struct* dd;	// current directory object

void SDCardManager_Init(void)
{
	while(!sd_raw_init()) {
		printf_P(PSTR("MMC/SD initialization failed\r\n"));
	}

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

	if(!partition) {
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
		if(!partition) {
#if DEBUG
			printf_P(PSTR("opening partition failed\r\n"));
#endif
			return;
		}
	}

	/* open file system */
	fs = fat_open(partition);
	if(!fs) {
#if DEBUG
		printf_P(PSTR("opening filesystem failed\r\n"));
#endif
		return;
	}

	/* open root directory */
	struct fat_dir_entry_struct directory;
	fat_get_dir_entry_of_path(fs, "/", &directory);

	dd = fat_open_dir(fs, &directory);
	if(!dd) {
#if DEBUG
		printf_P(PSTR("opening root directory failed\r\n"));
#endif
		return;
	}
}

const umeter_config const* UMeter_Init(void)
{
	struct fat_dir_entry_struct file_entry;

	// create data log file if it doesn't exist
	if(!fat_create_file(dd, "umeter.txt", &file_entry)) {
#if DEBUG
		printf_P(PSTR("error creating file 'umeter.txt'\r\n"));
#endif
	}

	// create config file if it doesn't exist
	if(!fat_create_file(dd, "umeter.ini", &file_entry)) {
#if DEBUG
		printf_P(PSTR("error creating file 'umeter.ini'\r\n"));
#endif
	}
	return get_umeter_ini(fs, dd);
}


void UMeter_Task(void)
{
	int32_t file_pos;		// file position
	unsigned int n, j, adc;	// n= number of bytes r/w, adc=conv val
	float volts, out;
	unsigned char buff[8];
	unsigned char* units;
	const umeter_config const* umeter;
#if DEBUG
	printf_P(PSTR("writing...\r\n"));
#endif
	// search file in current directory and open it
	struct fat_file_struct* fd = open_file_in_dir(fs, dd, "umeter.txt");
	if(!fd) {
#if DEBUG
		printf_P(PSTR("error opening file\r\n"));
#endif
		return;
	}

	// seek to EOF to append
	file_pos = -1;
	if(!fat_seek_file(fd, &file_pos, FAT_SEEK_END))
	{
#if DEBUG
	   printf_P(PSTR("Error seeking to EOF. Likely trying to seek on an empty file.\r\n"));
#endif
	}
	
	// check for newline char, add one if none
	if(file_pos > 0) {
		if(fat_read_file(fd, buff, 1) != 1) {
#if DEBUG
			printf_P(PSTR("error reading 1 byte\r\n"));
#endif
		}
		if(buff[0] != '\n') {
			if(fat_write_file(fd, "\n", 1) != 1) {
#if DEBUG
				printf_P(PSTR("error writing newline\r\n"));
#endif
			}
		}
	}

	// read sensor values
#if DEBUG
	printf("sensors: ");
#endif
	umeter = get_umeter_ini(fs, dd);
	for(j = 0; j < 4; j++) {
		if(!umeter->sensors[j].enabled) { // skip a sensor if it's disabled
			continue;
		}
		LED_ON();
		select_sensor(j+1);
		adc = adc_conversion();
		// v_in = ADC_value * Vref / (2^10)-1 * volt div. scaler
		volts = adc * 2.56 / 1023 * 2;
		if(umeter->sensors[j].raw_output) {
			out = volts;
			units = "V";
		}
		else {
			out = (volts - umeter->sensors[j].offset) / umeter->sensors[j].slope;
			units = umeter->sensors[j].units;
		}
		n = float2str(out, buff);
#if DEBUG
		printf("[%d: %s%s] ", j+1, buff, units);
#endif
		// write buff to file
		if(fat_write_file(fd, buff, n) != n) {
#if DEBUG
			printf_P(PSTR("error writing to file\r\n"));
#endif
			break;
		}
		LED_OFF();
	}
#if DEBUG
	printf("\r\n");
#endif
	// write newline
	if(fat_write_file(fd, "\n", 1) != 1) {
#if DEBUG
		printf_P(PSTR("error writing to file\r\n"));
#endif
	}
	fat_close_file(fd);
}

uint32_t SDCardManager_GetNbBlocks(void)
{
	uint32_t TotalBlocks = 0;

	if(CachedTotalBlocks != 0) {
		return CachedTotalBlocks;
	}

	if(!sd_raw_get_info(&disk_info)) {
#if DEBUG
		printf_P(PSTR("Error reading SD card info\r\n"));
#endif
		return 0;
	}

	CachedTotalBlocks = disk_info.capacity / 512;
	//printf_P(PSTR("SD blocks: %li\r\n"), TotalBlocks);

	return CachedTotalBlocks;
}

/** Writes blocks (OS blocks, not Dataflash pages) to the storage medium, the board dataflash IC(s), from
*  the pre-selected data OUT endpoint. This routine reads in OS sized blocks from the endpoint and writes
*  them to the dataflash in Dataflash page sized blocks.
*
*  \param[in] BlockAddress  Data block starting address for the write sequence
*  \param[in] TotalBlocks   Number of blocks of data to write
*/
uintptr_t SDCardManager_WriteBlockHandler(uint8_t* buffer, offset_t offset, void* p)
{
	/* Check if the endpoint is currently empty */
	if(!(Endpoint_IsReadWriteAllowed())) {
		/* Clear the current endpoint bank */
		Endpoint_ClearOUT();

		/* Wait until the host has sent another packet */
		if(Endpoint_WaitUntilReady()) {
			return 0;
		}
	}

	/* Write one 16-byte chunk of data to the dataflash */
	buffer[0] = Endpoint_Read_Byte();
	buffer[1] = Endpoint_Read_Byte();
	buffer[2] = Endpoint_Read_Byte();
	buffer[3] = Endpoint_Read_Byte();
	buffer[4] = Endpoint_Read_Byte();
	buffer[5] = Endpoint_Read_Byte();
	buffer[6] = Endpoint_Read_Byte();
	buffer[7] = Endpoint_Read_Byte();
	buffer[8] = Endpoint_Read_Byte();
	buffer[9] = Endpoint_Read_Byte();
	buffer[10] = Endpoint_Read_Byte();
	buffer[11] = Endpoint_Read_Byte();
	buffer[12] = Endpoint_Read_Byte();
	buffer[13] = Endpoint_Read_Byte();
	buffer[14] = Endpoint_Read_Byte();
	buffer[15] = Endpoint_Read_Byte();

	return 16;
}

void SDCardManager_WriteBlocks(uint32_t BlockAddress, uint16_t TotalBlocks)
{
	bool     UsingSecondBuffer   = false;
#if DEBUG
	//printf_P(PSTR("W %li %i\r\n"), BlockAddress, TotalBlocks);
#endif
	LED_ON();
	
	/* Wait until endpoint is ready before continuing */
	if(Endpoint_WaitUntilReady()) {
		return;
	}

	while(TotalBlocks) {
		sd_raw_write_interval(BlockAddress *  VIRTUAL_MEMORY_BLOCK_SIZE, Buffer, VIRTUAL_MEMORY_BLOCK_SIZE, &SDCardManager_WriteBlockHandler, NULL);

		/* Check if the current command is being aborted by the host */
		if(IsMassStoreReset) {
			return;
		}

		/* Decrement the blocks remaining counter and reset the sub block counter */
		BlockAddress++;
		TotalBlocks--;
	}

	/* If the endpoint is empty, clear it ready for the next packet from the host */
	if(!(Endpoint_IsReadWriteAllowed())) {
		Endpoint_ClearOUT();
	}
	LED_OFF();
}

/** Reads blocks (OS blocks, not Dataflash pages) from the storage medium, the board dataflash IC(s), into
*  the pre-selected data IN endpoint. This routine reads in Dataflash page sized blocks from the Dataflash
*  and writes them in OS sized blocks to the endpoint.
*
*  \param[in] BlockAddress  Data block starting address for the read sequence
*  \param[in] TotalBlocks   Number of blocks of data to read
*/

uint8_t SDCardManager_ReadBlockHandler(uint8_t* buffer, offset_t offset, void* p)
{
	uint8_t i;

	/* Check if the endpoint is currently full */
	if(!(Endpoint_IsReadWriteAllowed())) {
		/* Clear the endpoint bank to send its contents to the host */
		Endpoint_ClearIN();

		/* Wait until the endpoint is ready for more data */
		if(Endpoint_WaitUntilReady()) {
			return 0;
		}
	}

	Endpoint_Write_Byte(buffer[0]);
	Endpoint_Write_Byte(buffer[1]);
	Endpoint_Write_Byte(buffer[2]);
	Endpoint_Write_Byte(buffer[3]);
	Endpoint_Write_Byte(buffer[4]);
	Endpoint_Write_Byte(buffer[5]);
	Endpoint_Write_Byte(buffer[6]);
	Endpoint_Write_Byte(buffer[7]);
	Endpoint_Write_Byte(buffer[8]);
	Endpoint_Write_Byte(buffer[9]);
	Endpoint_Write_Byte(buffer[10]);
	Endpoint_Write_Byte(buffer[11]);
	Endpoint_Write_Byte(buffer[12]);
	Endpoint_Write_Byte(buffer[13]);
	Endpoint_Write_Byte(buffer[14]);
	Endpoint_Write_Byte(buffer[15]);

	/* Check if the current command is being aborted by the host */
	if(IsMassStoreReset) {
		return 0;
	}

	return 1;
}

void SDCardManager_ReadBlocks(uint32_t BlockAddress, uint16_t TotalBlocks)
{
	uint16_t CurrPage          = BlockAddress;
	uint16_t CurrPageByte      = 0;
#if DEBUG
	//printf_P(PSTR("R %li %i\r\n"), BlockAddress, TotalBlocks);
#endif

	LED_ON();
	/* Wait until endpoint is ready before continuing */
	if(Endpoint_WaitUntilReady()) {
		return;
	}

	while(TotalBlocks) {
		/* Read a data block from the SD card */
		sd_raw_read_interval(BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE, Buffer, 16, 512, &SDCardManager_ReadBlockHandler, NULL);

		/* Decrement the blocks remaining counter */
		BlockAddress++;
		TotalBlocks--;
	}

	/* If the endpoint is full, send its contents to the host */
	if(!(Endpoint_IsReadWriteAllowed())) {
		Endpoint_ClearIN();
	}
	LED_OFF();
}

/** Performs a simple test on the attached Dataflash IC(s) to ensure that they are working.
*
*  \return Boolean true if all media chips are working, false otherwise
*/
bool SDCardManager_CheckDataflashOperation(void)
{
	return true;
}
