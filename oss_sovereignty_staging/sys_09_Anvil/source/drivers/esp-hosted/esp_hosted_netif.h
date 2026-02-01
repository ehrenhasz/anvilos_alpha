 

#ifndef MICROPY_INCLUDED_DRIVERS_ESP_HOSTED_NETIF_H
#define MICROPY_INCLUDED_DRIVERS_ESP_HOSTED_NETIF_H

typedef struct esp_hosted_state esp_hosted_state_t;
int esp_hosted_netif_init(esp_hosted_state_t *state, uint32_t itf);
int esp_hosted_netif_deinit(esp_hosted_state_t *state, uint32_t itf);
int esp_hosted_netif_input(esp_hosted_state_t *state, uint32_t itf, const void *buf, size_t len);
err_t esp_hosted_netif_output(struct netif *netif, struct pbuf *p);

#endif 
