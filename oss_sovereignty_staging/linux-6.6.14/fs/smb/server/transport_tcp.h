 
 

#ifndef __KSMBD_TRANSPORT_TCP_H__
#define __KSMBD_TRANSPORT_TCP_H__

int ksmbd_tcp_set_interfaces(char *ifc_list, int ifc_list_sz);
int ksmbd_tcp_init(void);
void ksmbd_tcp_destroy(void);

#endif  
