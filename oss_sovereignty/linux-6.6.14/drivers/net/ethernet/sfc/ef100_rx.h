#ifndef EFX_EF100_RX_H
#define EFX_EF100_RX_H
#include "net_driver.h"
bool ef100_rx_buf_hash_valid(const u8 *prefix);
void efx_ef100_ev_rx(struct efx_channel *channel, const efx_qword_t *p_event);
void ef100_rx_write(struct efx_rx_queue *rx_queue);
void __ef100_rx_packet(struct efx_channel *channel);
#endif
