

#ifndef MICROPY_INCLUDED_RA_UART_H
#define MICROPY_INCLUDED_RA_UART_H

#include "shared/runtime/mpirq.h"
#include "pin.h"

typedef enum {
    HW_UART_0 = 0,
    HW_UART_1 = 1,
    HW_UART_2 = 2,
    HW_UART_3 = 3,
    HW_UART_4 = 4,
    HW_UART_5 = 5,
    HW_UART_6 = 6,
    HW_UART_7 = 7,
    HW_UART_8 = 8,
    HW_UART_9 = 9,
} machine_uart_t;

#define CHAR_WIDTH_8BIT (0)
#define CHAR_WIDTH_9BIT (1)

#define UART_WORDLENGTH_8B  (8)
#define UART_STOPBITS_1     (1)
#define UART_STOPBITS_2     (2)
#define UART_PARITY_NONE    (0)
#define UART_PARITY_ODD     (1)
#define UART_PARITY_EVEN    (2)

#define UART_HWCONTROL_CTS  (1)
#define UART_HWCONTROL_RTS  (2)


#define MP_UART_ALLOWED_FLAGS ((uint32_t)0x00000010)


#define MP_UART_RESERVED_FLAGS ((uint16_t)0x0020)

typedef struct _machine_uart_obj_t {
    mp_obj_base_t base;
    machine_uart_t uart_id : 8;
    uint32_t baudrate;
    const machine_pin_obj_t *rx;
    const machine_pin_obj_t *tx;
    const machine_pin_obj_t *cts;
    const machine_pin_obj_t *rts;
    uint8_t bits;
    uint8_t parity;
    uint8_t stop;
    uint8_t flow;
    bool is_static : 1;
    bool is_enabled : 1;
    bool attached_to_repl;              
    byte char_width;                    
    uint16_t char_mask;                 
    uint16_t timeout;                   
    uint16_t timeout_char;              
    uint16_t read_buf_len;              
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


void uart_attach_to_repl(machine_uart_obj_t *self, bool attached);
uint32_t uart_get_baudrate(machine_uart_obj_t *self);
mp_uint_t uart_rx_any(machine_uart_obj_t *uart_obj);
mp_uint_t uart_tx_avail(machine_uart_obj_t *uart_obj);
mp_uint_t uart_tx_busy(machine_uart_obj_t *uart_obj);
mp_uint_t uart_tx_txbuf(machine_uart_obj_t *self);
bool uart_rx_wait(machine_uart_obj_t *self, uint32_t timeout);
int uart_rx_char(machine_uart_obj_t *uart_obj);
bool uart_tx_wait(machine_uart_obj_t *self, uint32_t timeout);
size_t uart_tx_data(machine_uart_obj_t *self, const void *src_in, size_t num_chars, int *errcode);
void uart_tx_strn(machine_uart_obj_t *uart_obj, const char *str, uint len);

#endif 
