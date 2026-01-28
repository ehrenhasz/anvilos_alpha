

#include "user_interface.h"
#include "py/ringbuf.h"
#include "shared/runtime/interrupt_char.h"
#include "xtirq.h"

#define MICROPY_BEGIN_ATOMIC_SECTION() esp_disable_irq()
#define MICROPY_END_ATOMIC_SECTION(state) esp_enable_irq(state)

void mp_sched_keyboard_interrupt(void);

struct _mp_print_t;

extern const struct _mp_print_t mp_debug_print;

extern ringbuf_t stdin_ringbuf;

void mp_hal_signal_input(void);


extern int uart_attached_to_dupterm;

void mp_hal_init(void);
void mp_hal_rtc_init(void);

__attribute__((always_inline)) static inline uint32_t mp_hal_ticks_us(void) {
    return system_get_time();
}

__attribute__((always_inline)) static inline uint32_t mp_hal_ticks_cpu(void) {
    uint32_t ccount;
    __asm__ __volatile__ ("rsr %0,ccount" : "=a" (ccount));
    return ccount;
}

void mp_hal_delay_us(uint32_t);
void mp_hal_set_interrupt_char(int c);
uint32_t mp_hal_get_cpu_freq(void);

#define UART_TASK_ID 0
void uart_task_init();

uint32_t esp_disable_irq(void);
void esp_enable_irq(uint32_t state);


#include "osapi.h"
#define mp_hal_delay_us_fast(us) os_delay_us(us)

#define mp_hal_quiet_timing_enter() disable_irq()
#define mp_hal_quiet_timing_exit(irq_state) enable_irq(irq_state)


#include "etshal.h"
#include "gpio.h"
#include "modmachine.h"
#define MP_HAL_PIN_FMT "%u"
#define mp_hal_pin_obj_t uint32_t
#define mp_hal_get_pin_obj(o) mp_obj_get_pin(o)
#define mp_hal_pin_name(p) (p)
void mp_hal_pin_input(mp_hal_pin_obj_t pin);
void mp_hal_pin_output(mp_hal_pin_obj_t pin);
void mp_hal_pin_open_drain(mp_hal_pin_obj_t pin);
#define mp_hal_pin_od_low(p) do { \
        if ((p) == 16) { WRITE_PERI_REG(RTC_GPIO_ENABLE, (READ_PERI_REG(RTC_GPIO_ENABLE) & ~1) | 1); } \
        else { gpio_output_set(0, 1 << (p), 1 << (p), 0); } \
} while (0)
#define mp_hal_pin_od_high(p) do { \
        if ((p) == 16) { WRITE_PERI_REG(RTC_GPIO_ENABLE, (READ_PERI_REG(RTC_GPIO_ENABLE) & ~1)); } \
        else { gpio_output_set(0, 0, 0, 1 << (p));  } \
} while (0)

#define mp_hal_pin_od_high_dht(p) do { \
        if ((p) == 16) { WRITE_PERI_REG(RTC_GPIO_ENABLE, (READ_PERI_REG(RTC_GPIO_ENABLE) & ~1)); } \
        else { gpio_output_set(1 << (p), 0, 1 << (p), 0); } \
} while (0)
#define mp_hal_pin_read(p) pin_get(p)
#define mp_hal_pin_write(p, v) pin_set((p), (v))

void *ets_get_esf_buf_ctlblk(void);
int ets_esf_free_bufs(int idx);
