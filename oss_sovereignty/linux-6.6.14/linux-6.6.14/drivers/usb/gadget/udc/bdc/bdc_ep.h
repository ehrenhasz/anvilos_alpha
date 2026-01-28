#ifndef __LINUX_BDC_EP_H__
#define __LINUX_BDC_EP_H__
int bdc_init_ep(struct bdc *bdc);
int bdc_ep_disable(struct bdc_ep *ep);
int bdc_ep_enable(struct bdc_ep *ep);
void bdc_free_ep(struct bdc *bdc);
#endif  
