

#ifndef NRFX_GLUE_H
#define NRFX_GLUE_H

#include "py/mpconfig.h"
#include "py/misc.h"

#include <soc/nrfx_irqs.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE MP_ARRAY_SIZE
#endif

#define NRFX_STATIC_ASSERT(expression)

#define NRFX_ASSERT(expression)  do { bool res = expression; (void)res; } while (0)

void mp_hal_delay_us(mp_uint_t us);
#define NRFX_DELAY_US            mp_hal_delay_us

#if BLUETOOTH_SD

#if NRF51
#include "nrf_soc.h"
#else
#include "nrf_nvic.h"
#endif

#include "ble_drv.h"

#if (BLUETOOTH_SD == 110)
#define NRFX_IRQ_ENABLE(irq_number) \
    do { \
        if (ble_drv_stack_enabled() == 1) \
        { \
            sd_nvic_EnableIRQ(irq_number); \
        } else { \
            NVIC_EnableIRQ(irq_number); \
        } \
    } while (0)
#else
#define NRFX_IRQ_ENABLE(irq_number) sd_nvic_EnableIRQ(irq_number)
#endif

#if (BLUETOOTH_SD == 110)
#define NRFX_IRQ_DISABLE(irq_number) \
    do { \
        if (ble_drv_stack_enabled() == 1) \
        { \
            sd_nvic_DisableIRQ(irq_number);  \
        } else { \
            NVIC_DisableIRQ(irq_number); \
        } \
    } while (0)
#else
#define NRFX_IRQ_DISABLE(irq_number) sd_nvic_DisableIRQ(irq_number)
#endif

#if (BLUETOOTH_SD == 110)
#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) \
    do { \
        if (ble_drv_stack_enabled() == 1) \
        { \
            sd_nvic_SetPriority(irq_number, priority); \
        } else { \
            NVIC_SetPriority(irq_number, priority); \
        } \
    } while (0)
#else
#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) sd_nvic_SetPriority(irq_number, priority)
#endif

#if (BLUETOOTH_SD == 110)
#define NRFX_IRQ_PENDING_SET(irq_number) \
    do { \
        if (ble_drv_stack_enabled() == 1) \
        { \
            sd_nvic_SetPendingIRQ(irq_number); \
        } else { \
            NVIC_SetPendingIRQ(irq_number); \
        } \
    } while (0)
#else
#define NRFX_IRQ_PENDING_SET(irq_number) sd_nvic_SetPendingIRQ(irq_number)
#endif

#if (BLUETOOTH_SD == 110)
#define NRFX_IRQ_PENDING_CLEAR(irq_number) \
    do { \
        if (ble_drv_stack_enabled() == 1) \
        { \
            sd_nvic_ClearPendingIRQ(irq_number); \
        } else { \
            NVIC_ClearPendingIRQ(irq_number)(irq_number); \
        } \
    } while (0)
#else
#define NRFX_IRQ_PENDING_CLEAR(irq_number) sd_nvic_ClearPendingIRQ(irq_number)
#endif

#define NRFX_CRITICAL_SECTION_ENTER() \
    { \
        uint8_t _is_nested_critical_region; \
        sd_nvic_critical_region_enter(&_is_nested_critical_region);

#define NRFX_CRITICAL_SECTION_EXIT() \
    sd_nvic_critical_region_exit(_is_nested_critical_region); \
}

#else 

#define NRFX_IRQ_ENABLE(irq_number) NVIC_EnableIRQ(irq_number)
#define NRFX_IRQ_DISABLE(irq_number) NVIC_DisableIRQ(irq_number)
#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) NVIC_SetPriority(irq_number, priority)
#define NRFX_IRQ_PENDING_SET(irq_number) NVIC_SetPendingIRQ(irq_number)
#define NRFX_IRQ_PENDING_CLEAR(irq_number) NVIC_ClearPendingIRQ(irq_number)



#define NRFX_CRITICAL_SECTION_ENTER() { int _old_primask = __get_PRIMASK(); __disable_irq();
#define NRFX_CRITICAL_SECTION_EXIT() __set_PRIMASK(_old_primask); }

#endif 

#define NRFX_IRQ_IS_ENABLED(irq_number) (0 != (NVIC->ISER[irq_number / 32] & (1UL << (irq_number % 32))))

#endif 
