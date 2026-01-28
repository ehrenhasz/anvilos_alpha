

#ifndef RA_RA_GPT_H_
#define RA_RA_GPT_H_

#include <stdint.h>

void ra_gpt_timer_start(uint32_t ch);
void ra_gpt_timer_stop(uint32_t ch);
void ra_gpt_timer_set_freq(uint32_t ch, float freq);
float ra_gpt_timer_get_freq(uint32_t ch);
float ra_gpt_timer_tick_time(uint32_t ch);
void ra_gpt_timer_set_period(uint32_t ch, uint32_t ns);
uint32_t ra_gpt_timer_get_period(uint32_t ch);
void ra_gpt_timer_set_duty(uint32_t ch, uint8_t id, uint32_t duty);
uint32_t ra_gpt_timer_get_duty(uint32_t ch, uint8_t id);
void ra_gpt_timer_init(uint32_t pwm_pin, uint32_t ch, uint8_t id, uint32_t duty, float freq);
void ra_gpt_timer_deinit(uint32_t pwm_pin, uint32_t ch, uint8_t id);
bool ra_gpt_timer_is_pwm_pin(uint32_t pin);

#endif 
