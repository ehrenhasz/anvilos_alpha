
#ifndef MICROPY_INCLUDED_STM32_EXTINT_H
#define MICROPY_INCLUDED_STM32_EXTINT_H

#include "py/mphal.h"






#define EXTI_PVD_OUTPUT         (16)
#if defined(STM32L4)
#define EXTI_RTC_ALARM          (18)
#define EXTI_USB_OTG_FS_WAKEUP  (17)
#else
#define EXTI_RTC_ALARM          (17)
#define EXTI_USB_OTG_FS_WAKEUP  (18)
#endif
#define EXTI_ETH_WAKEUP         (19)
#define EXTI_USB_OTG_HS_WAKEUP  (20)
#if defined(STM32F0) || defined(STM32G4) || defined(STM32L1) || defined(STM32L4) || defined(STM32WL)
#define EXTI_RTC_TIMESTAMP      (19)
#define EXTI_RTC_WAKEUP         (20)
#elif defined(STM32H5)
#define EXTI_RTC_WAKEUP         (17)
#define EXTI_RTC_TAMP           (19)
#elif defined(STM32H7) || defined(STM32WB)
#define EXTI_RTC_TIMESTAMP      (18)
#define EXTI_RTC_WAKEUP         (19)
#elif defined(STM32G0)
#define EXTI_RTC_WAKEUP         (19)
#define EXTI_RTC_TIMESTAMP      (21)
#elif defined(STM32H5)
#define EXTI_RTC_WAKEUP         (17)
#else
#define EXTI_RTC_TIMESTAMP      (21)
#define EXTI_RTC_WAKEUP         (22)
#endif
#if defined(STM32F7)
#define EXTI_LPTIM1_ASYNC_EVENT (23)
#endif

#define EXTI_NUM_VECTORS        (PYB_EXTI_NUM_VECTORS)

void extint_init0(void);

uint extint_register(mp_obj_t pin_obj, uint32_t mode, uint32_t pull, mp_obj_t callback_obj, bool override_callback_obj);
void extint_register_pin(const machine_pin_obj_t *pin, uint32_t mode, bool hard_irq, mp_obj_t callback_obj);
void extint_set(const machine_pin_obj_t *pin, uint32_t mode);

void extint_enable(uint line);
void extint_disable(uint line);
void extint_swint(uint line);
void extint_trigger_mode(uint line, uint32_t mode);

void Handle_EXTI_Irq(uint32_t line);

extern const mp_obj_type_t extint_type;

#endif 
