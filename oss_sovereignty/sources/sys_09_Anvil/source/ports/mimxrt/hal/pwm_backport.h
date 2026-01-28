

#ifndef PWM_BACKPORT_H
#define PWM_BACKPORT_H
#include "fsl_pwm.h"
#ifdef FSL_FEATURE_SOC_TMR_COUNT
#include "fsl_qtmr.h"
#endif

typedef struct _pwm_signal_param_u16
{
    pwm_channels_t pwmChannel; 
    uint32_t dutyCycle_u16;    
    uint16_t Center_u16;       
    pwm_level_select_t level;  
    uint16_t deadtimeValue;    
} pwm_signal_param_u16_t;

#define PWM_FULL_SCALE  (65536UL)

void PWM_UpdatePwmDutycycle_u16(PWM_Type *base, pwm_submodule_t subModule,
    pwm_channels_t pwmSignal, uint32_t dutyCycle, uint16_t center);

void PWM_SetupPwm_u16(PWM_Type *base, pwm_submodule_t subModule, pwm_signal_param_u16_t *chnlParams,
    uint32_t pwmFreq_Hz, uint32_t srcClock_Hz, bool output_enable);

void PWM_SetupPwmx_u16(PWM_Type *base, pwm_submodule_t subModule,
    uint32_t pwmFreq_Hz, uint32_t duty_cycle, uint8_t invert, uint32_t srcClock_Hz);

#ifdef FSL_FEATURE_SOC_TMR_COUNT
status_t QTMR_SetupPwm_u16(TMR_Type *base, qtmr_channel_selection_t channel, uint32_t pwmFreqHz,
    uint32_t dutyCycleU16, bool outputPolarity, uint32_t srcClock_Hz, bool is_init);
#endif 

#endif 
