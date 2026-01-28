




#pragma once

#include "esp_netif.h"

#ifdef CONFIG_ESP_NETIF_TCPIP_LWIP

#include "lwip/netif.h"

typedef struct ppp_pcb_s ppp_pcb;

void pppapi_set_auth(ppp_pcb *pcb, u8_t authtype, const char *user, const char *passwd);

#endif
