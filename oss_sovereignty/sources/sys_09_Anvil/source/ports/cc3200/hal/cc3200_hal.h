

#include <stdint.h>
#include <stdbool.h>

#include "hal/utils.h"
#include "hal/systick.h"



#include "cc3200_asm.h"



#define HAL_FCPU_MHZ                        80U
#define HAL_FCPU_HZ                         (1000000U * HAL_FCPU_MHZ)
#define HAL_SYSTICK_PERIOD_US               1000U
#define UTILS_DELAY_US_TO_COUNT(us)         (((us) * HAL_FCPU_MHZ) / 6)

#define HAL_NVIC_INT_CTRL_REG               (*((volatile uint32_t *) 0xE000ED04 ) )
#define HAL_VECTACTIVE_MASK                 (0x1FUL)





#define HAL_INTRODUCE_SYNC_BARRIER() {                      \
                                        __asm(" dsb \n"     \
                                              " isb \n");   \
                                     }

#define MICROPY_BEGIN_ATOMIC_SECTION()              disable_irq()
#define MICROPY_END_ATOMIC_SECTION(state)           enable_irq(state)



extern void HAL_SystemInit (void);
extern void HAL_SystemDeInit (void);
extern void HAL_IncrementTick(void);
extern void mp_hal_set_interrupt_char (int c);

#define mp_hal_stdio_poll(poll_flags) (0) 
#define mp_hal_delay_us(usec) UtilsDelay(UTILS_DELAY_US_TO_COUNT(usec))
#define mp_hal_ticks_cpu() (SysTickPeriodGet() - SysTickValueGet())
#define mp_hal_time_ns() (0) 
