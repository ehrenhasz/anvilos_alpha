
#ifndef MICROPY_INCLUDED_STM32_UART_H
#define MICROPY_INCLUDED_STM32_UART_H

#include "shared/runtime/mpirq.h"

typedef enum {
    PYB_UART_NONE = 0,
    PYB_UART_1 = 1,
    PYB_UART_2 = 2,
    PYB_UART_3 = 3,
    PYB_UART_4 = 4,
    PYB_UART_5 = 5,
    PYB_UART_6 = 6,
    PYB_UART_7 = 7,
    PYB_UART_8 = 8,
    PYB_UART_9 = 9,
    PYB_UART_10 = 10,
    #ifdef LPUART1
    PYB_LPUART_1 = MICROPY_HW_MAX_UART + 1,
    #endif
    #ifdef LPUART2
    PYB_LPUART_2 = MICROPY_HW_MAX_UART + 2,
    #endif
} pyb_uart_t;

#define CHAR_WIDTH_8BIT (0)
#define CHAR_WIDTH_9BIT (1)


#define MP_UART_ALLOWED_FLAGS UART_FLAG_IDLE


#define MP_UART_RESERVED_FLAGS UART_FLAG_RXNE

typedef struct _machine_uart_obj_t {
    mp_obj_base_t base;
    USART_TypeDef *uartx;
    pyb_uart_t uart_id : 8;
    bool is_static : 1;
    bool is_enabled : 1;
    bool attached_to_repl;              
    byte char_width;                    
    uint16_t char_mask;                 
    uint16_t timeout;                   
    uint16_t timeout_char;              
    uint16_t read_buf_len;              
    volatile uint16_t read_buf_head;    
    uint16_t read_buf_tail;             
    byte *read_buf;                     
    uint16_t mp_irq_trigger;            
    uint16_t mp_irq_flags;              
    mp_irq_obj_t *mp_irq_obj;           
} machine_uart_obj_t;

extern const mp_irq_methods_t uart_irq_methods;

void uart_init0(void);
void uart_deinit_all(void);
bool uart_exists(int uart_id);
bool uart_init(machine_uart_obj_t *uart_obj,
    uint32_t baudrate, uint32_t bits, uint32_t parity, uint32_t stop, uint32_t flow);
void uart_irq_config(machine_uart_obj_t *self, bool enable);
void uart_set_rxbuf(machine_uart_obj_t *self, size_t len, void *buf);
void uart_deinit(machine_uart_obj_t *uart_obj);
void uart_irq_handler(mp_uint_t uart_id);

void uart_attach_to_repl(machine_uart_obj_t *self, bool attached);
uint32_t uart_get_source_freq(machine_uart_obj_t *self);
uint32_t uart_get_baudrate(machine_uart_obj_t *self);
void uart_set_baudrate(machine_uart_obj_t *self, uint32_t baudrate);

mp_uint_t uart_rx_any(machine_uart_obj_t *uart_obj);
bool uart_rx_wait(machine_uart_obj_t *self, uint32_t timeout);
int uart_rx_char(machine_uart_obj_t *uart_obj);
bool uart_tx_wait(machine_uart_obj_t *self, uint32_t timeout);
size_t uart_tx_data(machine_uart_obj_t *self, const void *src_in, size_t num_chars, int *errcode);
void uart_tx_strn(machine_uart_obj_t *uart_obj, const char *str, uint len);

static inline bool uart_tx_avail(machine_uart_obj_t *self) {
    #if defined(STM32F4) || defined(STM32L1)
    return self->uartx->SR & USART_SR_TXE;
    #elif defined(STM32G0) || defined(STM32H7) || defined(STM32WL)
    return self->uartx->ISR & USART_ISR_TXE_TXFNF;
    #else
    return self->uartx->ISR & USART_ISR_TXE;
    #endif
}

#endif 
