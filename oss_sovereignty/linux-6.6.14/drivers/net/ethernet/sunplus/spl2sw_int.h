 
 

#ifndef __SPL2SW_INT_H__
#define __SPL2SW_INT_H__

int spl2sw_rx_poll(struct napi_struct *napi, int budget);
int spl2sw_tx_poll(struct napi_struct *napi, int budget);
irqreturn_t spl2sw_ethernet_interrupt(int irq, void *dev_id);

#endif
