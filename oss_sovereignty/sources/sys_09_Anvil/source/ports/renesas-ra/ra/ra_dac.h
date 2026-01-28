



#ifndef RA_RA_DAC_H_
#define RA_RA_DAC_H_

#include <stdint.h>

void ra_dac_start(uint8_t ch);
void ra_dac_stop(uint8_t ch);
uint8_t ra_dac_is_running(uint8_t ch);
uint16_t ra_dac_read(uint8_t ch);
void ra_dac_write(uint8_t ch, uint16_t val);
void ra_dac_init(uint32_t dac_pin, uint8_t ch);
void ra_dac_deinit(uint32_t dac_pin, uint8_t ch);
bool ra_dac_is_dac_pin(uint32_t pin);

#endif 
