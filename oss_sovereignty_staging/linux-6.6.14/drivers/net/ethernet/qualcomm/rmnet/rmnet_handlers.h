 
 

#ifndef _RMNET_HANDLERS_H_
#define _RMNET_HANDLERS_H_

#include "rmnet_config.h"

void rmnet_egress_handler(struct sk_buff *skb);

rx_handler_result_t rmnet_rx_handler(struct sk_buff **pskb);

#endif  
