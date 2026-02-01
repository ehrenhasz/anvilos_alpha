 

#ifndef USNIC_TRANSPORT_H_
#define USNIC_TRANSPORT_H_

#include "usnic_abi.h"

const char *usnic_transport_to_str(enum usnic_transport_type trans_type);
 
int usnic_transport_sock_to_str(char *buf, int buf_sz,
					struct socket *sock);
 
u16 usnic_transport_rsrv_port(enum usnic_transport_type type, u16 port_num);
void usnic_transport_unrsrv_port(enum usnic_transport_type type, u16 port_num);
 
struct socket *usnic_transport_get_socket(int sock_fd);
void usnic_transport_put_socket(struct socket *sock);
 
int usnic_transport_sock_get_addr(struct socket *sock, int *proto,
					uint32_t *addr, uint16_t *port);
int usnic_transport_init(void);
void usnic_transport_fini(void);
#endif  
