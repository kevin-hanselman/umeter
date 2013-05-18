#ifndef __UMETER_ADC_H__
#define __UMETER_ADC_H__

#include <avr/io.h>

#define SENSOR_DDR	DDRF
#define SENSOR_PRT	PORTF

#define SENSOR1		PF5	//ADC5	// sensor port
#define SMUX1		0x5	// mux value to select adc input

#define SENSOR2		PF4	//ADC4
#define SMUX2		0x4

#define SENSOR3		PF1	//ADC1
#define SMUX3		0x1

#define SENSOR4		PF0	//ADC0
#define SMUX4		0x0

void adc_init(void);
unsigned int adc_conversion(void);
void select_sensor(int i);
void select_adc(unsigned char mux);
int float2str(float f, char* buff);

#endif