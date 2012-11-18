#include "umeter_adc.h"

void adc_init(void) {
  ADMUX  |=	(1 << REFS1) | (1 << REFS0);	// internal 2.56V reference
  ADMUX  &=	~(1 << ADLAR);	// right adjusted ADC result: ADCH -> 2 bits, ADCL -> 8 bits
  ADCSRA |=	(1 << ADEN);	// enable ADC
  ADCSRA &=	~(1 << ADATE);	// disable auto-triggering
  ADCSRA &=	~(1 << ADIE);	// disable ADC interrupt
  ADCSRA |=	((1 << ADPS2)|(1 << ADPS1)|(1 << ADPS0)); // prescaler division factor 2

  SENSOR_DDR &= ~((1 << SENSOR1)|(1 << SENSOR2)|(1 << SENSOR3)|(1 << SENSOR4)); // set sensor pins to inputs
  SENSOR_PRT &= ~((1 << SENSOR1)|(1 << SENSOR2)|(1 << SENSOR3)|(1 << SENSOR4)); // set pins low
}

unsigned int adc_conversion(void) {
  ADCSRA |= (1 << ADSC)|(1 << ADIF); // start conversion

  while( !(ADCSRA & (1 << ADIF)) ); // wait until ADIF (conversion done bit) is set

  return (ADCL | ((((unsigned int)ADCH) << 8) & 0x300) ); // MUST READ ADCL BEFORE ADCH
}

void select_adc(unsigned char mux) {
  ADMUX = (ADMUX & 0xE0) | (mux & 0x1F);
}

void select_sensor(int i) {
  switch(i) {
    case 1:
      select_adc(SMUX1);
      return;
    case 2:
      select_adc(SMUX2);
      return;
    case 3:
      select_adc(SMUX3);
      return;
    case 4:
      select_adc(SMUX4);
      return;
  }
}

int float2str(float f, char* buff) {
  int left = (int)f;		// integer section
  int right = (f - left)*1000;	// decimal section
  return sprintf(buff, "%d.%03d ", left, right);
}