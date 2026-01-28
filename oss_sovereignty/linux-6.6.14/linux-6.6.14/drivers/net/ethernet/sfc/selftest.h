#ifndef EFX_SELFTEST_H
#define EFX_SELFTEST_H
#include "net_driver.h"
struct efx_loopback_self_tests {
	int tx_sent[EFX_MAX_TXQ_PER_CHANNEL];
	int tx_done[EFX_MAX_TXQ_PER_CHANNEL];
	int rx_good;
	int rx_bad;
};
#define EFX_MAX_PHY_TESTS 20
struct efx_self_tests {
	int phy_alive;
	int nvram;
	int interrupt;
	int eventq_dma[EFX_MAX_CHANNELS];
	int eventq_int[EFX_MAX_CHANNELS];
	int memory;
	int registers;
	int phy_ext[EFX_MAX_PHY_TESTS];
	struct efx_loopback_self_tests loopback[LOOPBACK_TEST_MAX + 1];
};
void efx_loopback_rx_packet(struct efx_nic *efx, const char *buf_ptr,
			    int pkt_len);
int efx_selftest(struct efx_nic *efx, struct efx_self_tests *tests,
		 unsigned flags);
void efx_selftest_async_init(struct efx_nic *efx);
void efx_selftest_async_start(struct efx_nic *efx);
void efx_selftest_async_cancel(struct efx_nic *efx);
#endif  
