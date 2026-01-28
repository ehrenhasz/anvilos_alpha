

#ifndef PORTS_RA_RA_RA_SCI_H_
#define PORTS_RA_RA_RA_SCI_H_

#include <stdint.h>

#define RA_SCI_FLOW_START_NUM   (16)

typedef int (*SCI_CB)(uint32_t ch, uint32_t d);



bool ra_af_find_ch_af(ra_af_pin_t *af_pin, uint32_t size, uint32_t pin, uint32_t *ch, uint32_t *af);





void ra_sci_rx_set_callback(int ch, SCI_CB cb);


void ra_sci_rxirq_disable(uint32_t ch);
void ra_sci_rxirq_enable(uint32_t ch);
bool ra_sci_is_rxirq_enable(uint32_t ch);




void ra_sci_isr_te(uint32_t ch);
int ra_sci_rx_ch(uint32_t ch);
int ra_sci_rx_any(uint32_t ch);
int ra_sci_tx_busy(uint32_t ch);
int ra_sci_tx_bufsize(uint32_t ch);
void ra_sci_tx_ch(uint32_t ch, int c);
int ra_sci_tx_wait(uint32_t ch);
void ra_sci_tx_break(uint32_t ch);
void ra_sci_tx_str(uint32_t ch, uint8_t *p);

void ra_sci_txfifo_set(uint32_t ch, uint8_t *bufp, uint32_t size);
void ra_sci_rxfifo_set(uint32_t ch, uint8_t *bufp, uint32_t size);

void ra_sci_set_baud(uint32_t ch, uint32_t baud);
void ra_sci_init_with_flow(uint32_t ch, uint32_t tx_pin, uint32_t rx_pin, uint32_t baud, uint32_t bits, uint32_t parity, uint32_t stop, uint32_t flow, uint32_t cts_pin, uint32_t rts_pin);
void ra_sci_init(uint32_t ch, uint32_t tx_pin, uint32_t rx_pin, uint32_t baud, uint32_t bits, uint32_t parity, uint32_t stop, uint32_t flow);
void ra_sci_deinit(uint32_t ch);
void sci_uart_rxi_isr(void);
void sci_uart_txi_isr(void);
void sci_uart_eri_isr(void);
void sci_uart_tei_isr(void);

#endif 
