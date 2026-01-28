

#ifndef INCLUDED_MPHALPORT_H
#define INCLUDED_MPHALPORT_H

#include "py/ringbuf.h"
#include "shared/runtime/interrupt_char.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"

#define MICROPY_PLATFORM_VERSION "IDF" IDF_VER






#if CONFIG_FREERTOS_UNICORE
#define MP_TASK_COREID (0)
#else
#define MP_TASK_COREID (1)
#endif

extern TaskHandle_t mp_main_task_handle;

extern ringbuf_t stdin_ringbuf;


#if MICROPY_ERROR_REPORTING <= MICROPY_ERROR_REPORTING_NORMAL
#define check_esp_err(code) check_esp_err_(code)
void check_esp_err_(esp_err_t code);
#else
#define check_esp_err(code) check_esp_err_(code, __FUNCTION__, __LINE__, __FILE__)
void check_esp_err_(esp_err_t code, const char *func, const int line, const char *file);
#endif




#include "freertos/FreeRTOS.h"
#define MICROPY_BEGIN_ATOMIC_SECTION() portSET_INTERRUPT_MASK_FROM_ISR()
#define MICROPY_END_ATOMIC_SECTION(state) portCLEAR_INTERRUPT_MASK_FROM_ISR(state)

uint32_t mp_hal_ticks_us(void);
__attribute__((always_inline)) static inline uint32_t mp_hal_ticks_cpu(void) {
    uint32_t ccount;
    #if CONFIG_IDF_TARGET_ESP32C3
    __asm__ __volatile__ ("csrr %0, 0x7E2" : "=r" (ccount)); 
    #else
    __asm__ __volatile__ ("rsr %0,ccount" : "=a" (ccount));
    #endif
    return ccount;
}

void mp_hal_delay_us(uint32_t);
#define mp_hal_delay_us_fast(us) esp_rom_delay_us(us)
void mp_hal_set_interrupt_char(int c);
uint32_t mp_hal_get_cpu_freq(void);

#define mp_hal_quiet_timing_enter() MICROPY_BEGIN_ATOMIC_SECTION()
#define mp_hal_quiet_timing_exit(irq_state) MICROPY_END_ATOMIC_SECTION(irq_state)


void mp_hal_wake_main_task(void);
void mp_hal_wake_main_task_from_isr(void);


#include "py/obj.h"
#include "driver/gpio.h"
#define MP_HAL_PIN_FMT "%u"
#define mp_hal_pin_obj_t gpio_num_t
mp_hal_pin_obj_t machine_pin_get_id(mp_obj_t pin_in);
#define mp_hal_get_pin_obj(o) machine_pin_get_id(o)
#define mp_hal_pin_name(p) (p)
static inline void mp_hal_pin_input(mp_hal_pin_obj_t pin) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}
static inline void mp_hal_pin_output(mp_hal_pin_obj_t pin) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT);
}
static inline void mp_hal_pin_open_drain(mp_hal_pin_obj_t pin) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
}
static inline void mp_hal_pin_od_low(mp_hal_pin_obj_t pin) {
    gpio_set_level(pin, 0);
}
static inline void mp_hal_pin_od_high(mp_hal_pin_obj_t pin) {
    gpio_set_level(pin, 1);
}
static inline int mp_hal_pin_read(mp_hal_pin_obj_t pin) {
    return gpio_get_level(pin);
}
static inline void mp_hal_pin_write(mp_hal_pin_obj_t pin, int v) {
    gpio_set_level(pin, v);
}

spi_host_device_t machine_hw_spi_get_host(mp_obj_t in);

#endif 
