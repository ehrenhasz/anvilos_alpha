
 

#include <linux/kernel.h>
#include <net/sock.h>
#include <net/af_rxrpc.h>
#include "ar-internal.h"

 
unsigned int rxrpc_max_backlog __read_mostly = 10;

 
unsigned long rxrpc_soft_ack_delay = HZ;

 
unsigned long rxrpc_idle_ack_delay = HZ / 2;

 
unsigned int rxrpc_rx_window_size = 255;

 
unsigned int rxrpc_rx_mtu = 5692;

 
unsigned int rxrpc_rx_jumbo_max = 4;

#ifdef CONFIG_AF_RXRPC_INJECT_RX_DELAY
 
unsigned long rxrpc_inject_rx_delay;
#endif
