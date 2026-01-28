
#ifndef MICROPY_INCLUDED_STM32_I2CSLAVE_H
#define MICROPY_INCLUDED_STM32_I2CSLAVE_H

#include STM32_HAL_H

#if !defined(I2C2_BASE)

#define I2C2_BASE (I2C1_BASE + ((I2C3_BASE - I2C1_BASE) / 2))
#endif

typedef I2C_TypeDef i2c_slave_t;

void i2c_slave_init_helper(i2c_slave_t *i2c, int addr);

static inline void i2c_slave_init(i2c_slave_t *i2c, int irqn, int irq_pri, int addr) {
    int i2c_idx = ((uintptr_t)i2c - I2C1_BASE) / (I2C2_BASE - I2C1_BASE);
    #if defined(STM32F4) || defined(STM32F7)
    RCC->APB1ENR |= 1 << (RCC_APB1ENR_I2C1EN_Pos + i2c_idx);
    volatile uint32_t tmp = RCC->APB1ENR; 
    (void)tmp;
    #elif defined(STM32G0)
    RCC->APBENR1 |= 1 << (RCC_APBENR1_I2C1EN_Pos + i2c_idx);
    volatile uint32_t tmp = RCC->APBENR1; 
    (void)tmp;
    #elif defined(STM32H5) || defined(STM32H7)
    RCC->APB1LENR |= 1 << (RCC_APB1LENR_I2C1EN_Pos + i2c_idx);
    volatile uint32_t tmp = RCC->APB1LENR; 
    (void)tmp;
    #elif defined(STM32WB)
    RCC->APB1ENR1 |= 1 << (RCC_APB1ENR1_I2C1EN_Pos + i2c_idx);
    volatile uint32_t tmp = RCC->APB1ENR1; 
    (void)tmp;
    #endif

    i2c_slave_init_helper(i2c, addr);

    NVIC_SetPriority(irqn, irq_pri);
    NVIC_EnableIRQ(irqn);
}

static inline void i2c_slave_shutdown(i2c_slave_t *i2c, int irqn) {
    i2c->CR1 = 0;
    NVIC_DisableIRQ(irqn);
}

void i2c_slave_ev_irq_handler(i2c_slave_t *i2c);


int i2c_slave_process_addr_match(i2c_slave_t *i2c, int rw);
int i2c_slave_process_rx_byte(i2c_slave_t *i2c, uint8_t val);
void i2c_slave_process_rx_end(i2c_slave_t *i2c);
uint8_t i2c_slave_process_tx_byte(i2c_slave_t *i2c);
void i2c_slave_process_tx_end(i2c_slave_t *i2c);

#endif 
