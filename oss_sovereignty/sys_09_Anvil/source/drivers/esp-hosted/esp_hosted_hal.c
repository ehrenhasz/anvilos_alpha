 

#include "py/mphal.h"

#if MICROPY_PY_NETWORK_ESP_HOSTED

#include <stdint.h>
#include <string.h>

#include "py/runtime.h"
#include "extmod/modmachine.h"
#ifdef MICROPY_HW_WIFI_LED
#include "led.h"
#endif

#include "esp_hosted_hal.h"
#include "esp_hosted_wifi.h"

extern void mod_network_poll_events(void);

static mp_obj_t esp_hosted_pin_irq_callback(mp_obj_t self_in) {
    #ifdef MICROPY_HW_WIFI_LED
    led_toggle(MICROPY_HW_WIFI_LED);
    #endif
    mod_network_poll_events();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(esp_hosted_pin_irq_callback_obj, esp_hosted_pin_irq_callback);

MP_WEAK int esp_hosted_hal_init(uint32_t mode) {
    
    esp_hosted_hal_deinit();

    if (mode == ESP_HOSTED_MODE_BT) {
        
        return 0;
    }

    mp_hal_pin_input(MICROPY_HW_WIFI_HANDSHAKE);
    mp_hal_pin_input(MICROPY_HW_WIFI_DATAREADY);

    
    mp_obj_t args[] = {
        MP_OBJ_NEW_SMALL_INT(MICROPY_HW_WIFI_SPI_ID),
        MP_OBJ_NEW_SMALL_INT(MICROPY_HW_WIFI_SPI_BAUDRATE),
        MP_OBJ_NEW_QSTR(MP_QSTR_phase), MP_OBJ_NEW_SMALL_INT(0),
        MP_OBJ_NEW_QSTR(MP_QSTR_polarity), MP_OBJ_NEW_SMALL_INT(1),
    };

    MP_STATE_PORT(mp_wifi_spi) =
        MP_OBJ_TYPE_GET_SLOT(&machine_spi_type, make_new)((mp_obj_t)&machine_spi_type, 2, 2, args);

    
    
    mp_hal_pin_output(MICROPY_HW_WIFI_SPI_CS);
    mp_hal_pin_write(MICROPY_HW_WIFI_SPI_CS, 1);
    return 0;
}

MP_WEAK int esp_hosted_hal_deinit(void) {
    
    esp_hosted_hal_irq_enable(false);

    
    esp_hosted_wifi_deinit();

    mp_hal_pin_output(MICROPY_HW_ESP_HOSTED_GPIO0);
    mp_hal_pin_output(MICROPY_HW_ESP_HOSTED_RESET);

    #ifndef MICROPY_HW_ESP_HOSTED_SHARED_PINS
    mp_hal_pin_output(MICROPY_HW_WIFI_SPI_CS);
    mp_hal_pin_write(MICROPY_HW_WIFI_SPI_CS, 1);
    #endif

    
    mp_hal_pin_write(MICROPY_HW_ESP_HOSTED_GPIO0, 1);
    mp_hal_pin_write(MICROPY_HW_ESP_HOSTED_RESET, 0);
    mp_hal_delay_ms(100);
    mp_hal_pin_write(MICROPY_HW_ESP_HOSTED_RESET, 1);
    mp_hal_delay_ms(500);

    MP_STATE_PORT(mp_wifi_spi) = MP_OBJ_NULL;
    return 0;
}

MP_WEAK void esp_hosted_hal_irq_enable(bool enable) {
    #ifdef MICROPY_HW_WIFI_IRQ_PIN
    
    mp_obj_t pin_args[] = {
        NULL, 
        (mp_obj_t)MICROPY_HW_WIFI_IRQ_PIN,   
        mp_const_none  
    };
    mp_load_method_maybe((mp_obj_t)MICROPY_HW_WIFI_IRQ_PIN, MP_QSTR_irq, pin_args);
    if (pin_args[0] && pin_args[1]) {
        mp_call_method_n_kw(1, 0, pin_args);
    }

    if (enable) {
        
        mp_obj_t irq_rising_attr[2];
        mp_load_method_maybe((mp_obj_t)MICROPY_HW_WIFI_IRQ_PIN, MP_QSTR_IRQ_RISING, irq_rising_attr);

        if (irq_rising_attr[0] != MP_OBJ_NULL && irq_rising_attr[1] == MP_OBJ_NULL) {
            
            mp_obj_t pin_args[] = {
                NULL, 
                (mp_obj_t)MICROPY_HW_WIFI_IRQ_PIN,   
                (mp_obj_t)&esp_hosted_pin_irq_callback_obj,   
                NULL,  
                mp_const_true,  
            };
            pin_args[3] = irq_rising_attr[0];
            mp_load_method_maybe((mp_obj_t)MICROPY_HW_WIFI_IRQ_PIN, MP_QSTR_irq, pin_args);
            if (pin_args[0] != MP_OBJ_NULL && pin_args[1] != MP_OBJ_NULL) {
                mp_call_method_n_kw(3, 0, pin_args);
            }
        }
    }
    #endif
}

MP_WEAK int esp_hosted_hal_atomic_enter(void) {
    #if MICROPY_ENABLE_SCHEDULER
    mp_sched_lock();
    #endif
    return 0;
}

MP_WEAK int esp_hosted_hal_atomic_exit(void) {
    #if MICROPY_ENABLE_SCHEDULER
    mp_sched_unlock();
    #endif
    return 0;
}

MP_WEAK bool esp_hosted_hal_data_ready(void) {
    return mp_hal_pin_read(MICROPY_HW_WIFI_DATAREADY);
}

MP_WEAK int esp_hosted_hal_spi_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t size) {
    mp_obj_t mp_wifi_spi = MP_STATE_PORT(mp_wifi_spi);
    const mp_machine_spi_p_t *spi_proto = MP_OBJ_TYPE_GET_SLOT(&machine_spi_type, protocol);

    
    for (mp_uint_t start = mp_hal_ticks_ms(); ; mp_hal_delay_ms(1)) {
        if (mp_hal_pin_read(MICROPY_HW_WIFI_HANDSHAKE) &&
           (rx_buf == NULL || mp_hal_pin_read(MICROPY_HW_WIFI_DATAREADY))) {
            break;
        }
        if ((mp_hal_ticks_ms() - start) >= 1000) {
            error_printf("timeout waiting for handshake\n");
            return -1;
        }
    }

    mp_hal_pin_write(MICROPY_HW_WIFI_SPI_CS, 0);
    mp_hal_delay_us(10);
    spi_proto->transfer(mp_wifi_spi, size, tx_buf, rx_buf);
    mp_hal_pin_write(MICROPY_HW_WIFI_SPI_CS, 1);
    mp_hal_delay_us(100);

    if (esp_hosted_hal_data_ready()) {
        mod_network_poll_events();
    }
    return 0;
}

MP_WEAK void *esp_hosted_hal_alloc(void *user, size_t size) {
    (void)user;
    void *mem = m_malloc0(size);
    return mem;
}

MP_WEAK void esp_hosted_hal_free(void *user, void *ptr) {
    (void)user;
    m_free(ptr);
}

MP_WEAK void *esp_hosted_hal_calloc(size_t nmemb, size_t size) {
    return NULL;
}

MP_WEAK void *esp_hosted_hal_realloc(void *ptr, size_t size) {
    return NULL;
}



MP_WEAK void *malloc(size_t size) {
    (void)size;
    debug_printf("system malloc called\n");
    return NULL;
}

MP_WEAK void free(void *ptr) {
    (void)ptr;
    debug_printf("system free called\n");
}

#endif 
