
#ifndef MICROPY_INCLUDED_STM32_TIMER_H
#define MICROPY_INCLUDED_STM32_TIMER_H

extern TIM_HandleTypeDef TIM5_Handle;

extern const mp_obj_type_t pyb_timer_type;

void timer_init0(void);
void timer_tim5_init(void);
TIM_HandleTypeDef *timer_tim6_init(uint freq);
void timer_deinit(void);
uint32_t timer_get_source_freq(uint32_t tim_id);
void timer_irq_handler(uint tim_id);

TIM_HandleTypeDef *pyb_timer_get_handle(mp_obj_t timer);

#endif 
