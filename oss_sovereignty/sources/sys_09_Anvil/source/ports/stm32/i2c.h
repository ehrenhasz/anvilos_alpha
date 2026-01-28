
#ifndef MICROPY_INCLUDED_STM32_I2C_H
#define MICROPY_INCLUDED_STM32_I2C_H

#include "dma.h"


#define PYB_I2C_MASTER_ADDRESS (0xfe)

typedef struct _pyb_i2c_obj_t {
    mp_obj_base_t base;
    I2C_HandleTypeDef *i2c;
    const dma_descr_t *tx_dma_descr;
    const dma_descr_t *rx_dma_descr;
    bool *use_dma;
} pyb_i2c_obj_t;

extern I2C_HandleTypeDef I2CHandle1;
extern I2C_HandleTypeDef I2CHandle2;
extern I2C_HandleTypeDef I2CHandle3;
extern I2C_HandleTypeDef I2CHandle4;
extern const mp_obj_type_t pyb_i2c_type;
extern const pyb_i2c_obj_t pyb_i2c_obj[4];

void i2c_init0(void);
int pyb_i2c_init(I2C_HandleTypeDef *i2c);
int pyb_i2c_init_freq(const pyb_i2c_obj_t *self, mp_int_t freq);
uint32_t pyb_i2c_get_baudrate(I2C_HandleTypeDef *i2c);
void i2c_ev_irq_handler(mp_uint_t i2c_id);
void i2c_er_irq_handler(mp_uint_t i2c_id);

typedef I2C_TypeDef i2c_t;

int i2c_init(i2c_t *i2c, mp_hal_pin_obj_t scl, mp_hal_pin_obj_t sda, uint32_t freq, uint16_t timeout);
int i2c_start_addr(i2c_t *i2c, int rd_wrn, uint16_t addr, size_t len, bool stop);
int i2c_read(i2c_t *i2c, uint8_t *dest, size_t len, size_t next_len);
int i2c_write(i2c_t *i2c, const uint8_t *src, size_t len, size_t next_len);
int i2c_readfrom(i2c_t *i2c, uint16_t addr, uint8_t *dest, size_t len, bool stop);
int i2c_writeto(i2c_t *i2c, uint16_t addr, const uint8_t *src, size_t len, bool stop);

int i2c_find_peripheral(mp_obj_t id);

#endif 
