

#ifndef RA_ADC_H_
#define RA_ADC_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(RA4M1) | defined(RA4W1)
#define ADC_RESOLUTION (14)
#else
#define ADC_RESOLUTION (12)
#endif

enum ADC14_PIN
{
    #if defined(RA4M1)

    AN000 = 0,
    AN001 = 1,
    AN002 = 2,
    AN003 = 3,
    AN004 = 4,
    AN005 = 5,
    AN006 = 6,
    AN007 = 7,
    AN008 = 8,
    AN009 = 9,
    AN010 = 10,
    AN011 = 11,
    AN012 = 12,
    AN013 = 13,
    AN014 = 14,
    AN016 = 16,
    AN017 = 17,
    AN018 = 18,
    AN019 = 19,
    AN020 = 20,
    AN021 = 21,
    AN022 = 22,
    AN023 = 23,
    AN024 = 24,
    AN025 = 25,

    #elif defined(RA4W1)

    AN004 = 4,
    AN005 = 5,
    AN006 = 6,
    AN009 = 9,
    AN010 = 10,
    AN017 = 17,
    AN019 = 19,
    AN020 = 20,

    #elif defined(RA6M1)

    AN000 = 0,
    AN001 = 1,
    AN002 = 2,
    AN003 = 3,
    AN004 = 4,
    AN005 = 5,
    AN006 = 6,
    AN007 = 7,
    AN008 = 8,
    AN009 = 9,
    AN010 = 10,
    AN011 = 11,
    AN012 = 12,
    AN013 = 13,
    AN014 = 14,
    AN015 = 15,
    AN016 = 16,
    AN017 = 17,
    AN018 = 18,
    AN019 = 19,
    AN020 = 20,
    AN021 = 21,
    AN022 = 22,
    AN100 = 32,
    AN101 = 33,
    AN102 = 34,
    AN103 = 35,
    AN104 = 36,
    AN105 = 37,
    AN106 = 38,
    AN107 = 39,
    AN108 = 40,
    AN109 = 41,
    AN110 = 42,
    AN111 = 43,
    AN112 = 44,
    AN113 = 45,
    AN114 = 46,
    AN115 = 47,
    AN116 = 48,
    AN117 = 49,
    AN118 = 50,
    AN119 = 51,
    AN120 = 52,

    #elif defined(RA6M2)
    
    AN000 = 0,
    AN001 = 1,
    AN002 = 2,
    AN003 = 3,
    AN004 = 4,
    AN005 = 5,
    AN006 = 6,
    AN007 = 7,
    AN016 = 16,
    AN017 = 17,
    AN018 = 18,
    AN019 = 19,
    AN020 = 20,
    
    AN100 = 32,
    AN101 = 33,
    AN102 = 34,
    AN105 = 37,
    AN106 = 38,
    AN107 = 39,
    AN116 = 48,
    AN117 = 49,
    AN118 = 50,

    #elif defined(RA6M3)
    
    AN000 = 0,
    AN001 = 1,
    AN002 = 2,
    AN003 = 3,
    AN004 = 4,
    AN005 = 5,
    AN006 = 6,
    AN007 = 7,
    AN016 = 16,
    AN017 = 17,
    AN018 = 18,
    AN019 = 19,
    AN020 = 20,
    
    AN100 = 32,
    AN101 = 33,
    AN102 = 34,
    AN103 = 35,
    AN105 = 37,
    AN106 = 38,
    AN107 = 39,
    AN116 = 48,
    AN117 = 49,
    AN118 = 50,
    AN119 = 51,

    #elif defined(RA6M5)
    
    AN000 = 0,
    AN001 = 1,
    AN002 = 2,
    AN003 = 3,
    AN004 = 4,
    AN005 = 5,
    AN006 = 6,
    AN007 = 7,
    AN008 = 8,
    AN009 = 9,
    AN010 = 10,
    AN012 = 12,
    AN013 = 13,
    
    AN100 = 32,
    AN101 = 33,
    AN102 = 34,
    AN116 = 35,
    AN117 = 37,
    AN118 = 38,
    AN119 = 39,
    AN120 = 48,
    AN121 = 49,
    AN122 = 50,
    AN123 = 51,
    AN124 = 52,
    AN125 = 53,
    AN126 = 54,
    AN127 = 55,
    AN128 = 56,

    #else
    #error "CMSIS MCU Series is not specified."
    #endif
    ADC_TEMP = 29,
    ADC_REF = 30,
    ADC_NON = 255,
};

#if defined(RA4M1) | defined(RA4W1)
#define RA_ADC_DEF_RESOLUTION 14
#else
#define RA_ADC_DEF_RESOLUTION 12
#endif

bool ra_adc_pin_to_ch(uint32_t pin, uint8_t *ch);
bool ra_adc_ch_to_pin(uint8_t ch, uint32_t *pin);
uint8_t ra_adc_get_channel(uint32_t pin);


void ra_adc_set_pin(uint32_t pin, bool adc_enable);
void ra_adc_enable(uint32_t pin);
void ra_adc_disable(uint32_t pin);
void ra_adc_set_resolution_set(uint8_t res);
uint8_t ra_adc_get_resolution(void);
uint16_t ra_adc_read_ch(uint8_t ch);
uint16_t ra_adc_read(uint32_t pin);
int16_t ra_adc_read_itemp(void);
float ra_adc_read_ftemp(void);
float ra_adc_read_fref(void);
void ra_adc_all(uint32_t resolution, uint32_t mask);
uint16_t ra_adc_all_read_ch(uint32_t ch);
bool ra_adc_init(void);
bool ra_adc_deinit(void);
__attribute__((weak)) void adc_scan_end_isr(void);

#endif 
