 

#ifndef MICROPY_INCLUDED_EXTMOD_MODMACHINE_H
#define MICROPY_INCLUDED_EXTMOD_MODMACHINE_H

#include "py/mphal.h"
#include "py/obj.h"

#if MICROPY_PY_MACHINE_SPI || MICROPY_PY_MACHINE_SOFTSPI
#include "drivers/bus/spi.h"
#endif



#ifndef MICROPY_PY_MACHINE_ADC_INIT
#define MICROPY_PY_MACHINE_ADC_INIT (0)
#endif



#ifndef MICROPY_PY_MACHINE_ADC_DEINIT
#define MICROPY_PY_MACHINE_ADC_DEINIT (0)
#endif



#ifndef MICROPY_PY_MACHINE_ADC_BLOCK
#define MICROPY_PY_MACHINE_ADC_BLOCK (0)
#endif



#ifndef MICROPY_PY_MACHINE_ADC_READ_UV
#define MICROPY_PY_MACHINE_ADC_READ_UV (0)
#endif



#ifndef MICROPY_PY_MACHINE_ADC_ATTEN_WIDTH
#define MICROPY_PY_MACHINE_ADC_ATTEN_WIDTH (0)
#endif



#ifndef MICROPY_PY_MACHINE_ADC_READ
#define MICROPY_PY_MACHINE_ADC_READ (0)
#endif



#ifndef MICROPY_PY_MACHINE_UART_SENDBREAK
#define MICROPY_PY_MACHINE_UART_SENDBREAK (0)
#endif



#ifndef MICROPY_PY_MACHINE_UART_READCHAR_WRITECHAR
#define MICROPY_PY_MACHINE_UART_READCHAR_WRITECHAR (0)
#endif



#ifndef MICROPY_PY_MACHINE_UART_IRQ
#define MICROPY_PY_MACHINE_UART_IRQ (0)
#endif


#define MP_MACHINE_I2C_CHECK_FOR_LEGACY_SOFTI2C_CONSTRUCTION(n_args, n_kw, all_args) \
    do { \
        if (n_args == 0 || all_args[0] == MP_OBJ_NEW_SMALL_INT(-1)) { \
            mp_print_str(MICROPY_ERROR_PRINTER, "Warning: I2C(-1, ...) is deprecated, use SoftI2C(...) instead\n"); \
            if (n_args != 0) { \
                --n_args; \
                ++all_args; \
            } \
            return MP_OBJ_TYPE_GET_SLOT(&mp_machine_soft_i2c_type, make_new)(&mp_machine_soft_i2c_type, n_args, n_kw, all_args); \
        } \
    } while (0)


#define MP_MACHINE_SPI_CHECK_FOR_LEGACY_SOFTSPI_CONSTRUCTION(n_args, n_kw, all_args) \
    do { \
        if (n_args == 0 || all_args[0] == MP_OBJ_NEW_SMALL_INT(-1)) { \
            mp_print_str(MICROPY_ERROR_PRINTER, "Warning: SPI(-1, ...) is deprecated, use SoftSPI(...) instead\n"); \
            if (n_args != 0) { \
                --n_args; \
                ++all_args; \
            } \
            return MP_OBJ_TYPE_GET_SLOT(&mp_machine_soft_spi_type, make_new)(&mp_machine_soft_spi_type, n_args, n_kw, all_args); \
        } \
    } while (0)

#if MICROPY_PY_MACHINE_I2C || MICROPY_PY_MACHINE_SOFTI2C

#define MP_MACHINE_I2C_FLAG_READ (0x01) 
#define MP_MACHINE_I2C_FLAG_STOP (0x02)

#if MICROPY_PY_MACHINE_I2C_TRANSFER_WRITE1

#define MP_MACHINE_I2C_FLAG_WRITE1 (0x04)
#endif

#endif


typedef struct _machine_adc_obj_t machine_adc_obj_t;
typedef struct _machine_adc_block_obj_t machine_adc_block_obj_t;
typedef struct _machine_i2s_obj_t machine_i2s_obj_t;
typedef struct _machine_pwm_obj_t machine_pwm_obj_t;
typedef struct _machine_uart_obj_t machine_uart_obj_t;
typedef struct _machine_wdt_obj_t machine_wdt_obj_t;

typedef struct _machine_mem_obj_t {
    mp_obj_base_t base;
    unsigned elem_size; 
} machine_mem_obj_t;

#if MICROPY_PY_MACHINE_I2C || MICROPY_PY_MACHINE_SOFTI2C

typedef struct _mp_machine_i2c_buf_t {
    size_t len;
    uint8_t *buf;
} mp_machine_i2c_buf_t;






typedef struct _mp_machine_i2c_p_t {
    #if MICROPY_PY_MACHINE_I2C_TRANSFER_WRITE1
    bool transfer_supports_write1;
    #endif
    void (*init)(mp_obj_base_t *obj, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
    int (*start)(mp_obj_base_t *obj);
    int (*stop)(mp_obj_base_t *obj);
    int (*read)(mp_obj_base_t *obj, uint8_t *dest, size_t len, bool nack);
    int (*write)(mp_obj_base_t *obj, const uint8_t *src, size_t len);
    int (*transfer)(mp_obj_base_t *obj, uint16_t addr, size_t n, mp_machine_i2c_buf_t *bufs, unsigned int flags);
    int (*transfer_single)(mp_obj_base_t *obj, uint16_t addr, size_t len, uint8_t *buf, unsigned int flags);
} mp_machine_i2c_p_t;


typedef struct _mp_machine_soft_i2c_obj_t {
    mp_obj_base_t base;
    uint32_t us_delay;
    uint32_t us_timeout;
    mp_hal_pin_obj_t scl;
    mp_hal_pin_obj_t sda;
} mp_machine_soft_i2c_obj_t;

#endif

#if MICROPY_PY_MACHINE_SPI || MICROPY_PY_MACHINE_SOFTSPI


typedef struct _mp_machine_spi_p_t {
    void (*init)(mp_obj_base_t *obj, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
    void (*deinit)(mp_obj_base_t *obj); 
    void (*transfer)(mp_obj_base_t *obj, size_t len, const uint8_t *src, uint8_t *dest);
} mp_machine_spi_p_t;


typedef struct _mp_machine_soft_spi_obj_t {
    mp_obj_base_t base;
    mp_soft_spi_obj_t spi;
} mp_machine_soft_spi_obj_t;

#endif


extern const machine_mem_obj_t machine_mem8_obj;
extern const machine_mem_obj_t machine_mem16_obj;
extern const machine_mem_obj_t machine_mem32_obj;




extern const mp_obj_type_t machine_adc_type;
extern const mp_obj_type_t machine_adc_block_type;
extern const mp_obj_type_t machine_i2c_type;
extern const mp_obj_type_t machine_i2s_type;
extern const mp_obj_type_t machine_mem_type;
extern const mp_obj_type_t machine_pin_type;
extern const mp_obj_type_t machine_pinbase_type;
extern const mp_obj_type_t machine_pwm_type;
extern const mp_obj_type_t machine_rtc_type;
extern const mp_obj_type_t machine_signal_type;
extern const mp_obj_type_t machine_spi_type;
extern const mp_obj_type_t machine_timer_type;
extern const mp_obj_type_t machine_uart_type;
extern const mp_obj_type_t machine_usbd_type;
extern const mp_obj_type_t machine_wdt_type;

#if MICROPY_PY_MACHINE_SOFTI2C
extern const mp_obj_type_t mp_machine_soft_i2c_type;
#endif
#if MICROPY_PY_MACHINE_I2C || MICROPY_PY_MACHINE_SOFTI2C
extern const mp_obj_dict_t mp_machine_i2c_locals_dict;
#endif

#if MICROPY_PY_MACHINE_SOFTSPI
extern const mp_obj_type_t mp_machine_soft_spi_type;
extern const mp_machine_spi_p_t mp_machine_soft_spi_p;
#endif
#if MICROPY_PY_MACHINE_SPI || MICROPY_PY_MACHINE_SOFTSPI
extern const mp_obj_dict_t mp_machine_spi_locals_dict;
#endif

#if MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE
extern const mp_obj_type_t machine_usb_device_type;
#endif

#if defined(MICROPY_MACHINE_MEM_GET_READ_ADDR)
uintptr_t MICROPY_MACHINE_MEM_GET_READ_ADDR(mp_obj_t addr_o, uint align);
#endif
#if defined(MICROPY_MACHINE_MEM_GET_WRITE_ADDR)
uintptr_t MICROPY_MACHINE_MEM_GET_WRITE_ADDR(mp_obj_t addr_o, uint align);
#endif

NORETURN mp_obj_t machine_bootloader(size_t n_args, const mp_obj_t *args);
void machine_bitstream_high_low(mp_hal_pin_obj_t pin, uint32_t *timing_ns, const uint8_t *buf, size_t len);
mp_uint_t machine_time_pulse_us(mp_hal_pin_obj_t pin, int pulse_level, mp_uint_t timeout_us);

MP_DECLARE_CONST_FUN_OBJ_0(machine_unique_id_obj);
MP_DECLARE_CONST_FUN_OBJ_0(machine_reset_obj);
MP_DECLARE_CONST_FUN_OBJ_0(machine_reset_cause_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_freq_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lightsleep_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_deepsleep_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_bootloader_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_bitstream_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(machine_time_pulse_us_obj);

#if MICROPY_PY_MACHINE_I2C
int mp_machine_i2c_transfer_adaptor(mp_obj_base_t *self, uint16_t addr, size_t n, mp_machine_i2c_buf_t *bufs, unsigned int flags);
int mp_machine_soft_i2c_transfer(mp_obj_base_t *self, uint16_t addr, size_t n, mp_machine_i2c_buf_t *bufs, unsigned int flags);
#endif

#if MICROPY_PY_MACHINE_SPI
mp_obj_t mp_machine_spi_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_machine_spi_read_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_machine_spi_readinto_obj);
MP_DECLARE_CONST_FUN_OBJ_2(mp_machine_spi_write_obj);
MP_DECLARE_CONST_FUN_OBJ_3(mp_machine_spi_write_readinto_obj);
#endif

#endif 
