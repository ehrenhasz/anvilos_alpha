 

 

#ifndef _CANOHOST_H
#define _CANOHOST_H

char		*get_peer_ipaddr(int);
int		 get_peer_port(int);
char		*get_local_ipaddr(int);
char		*get_local_name(int);
int		get_local_port(int);

#endif  

void		 ipv64_normalise_mapped(struct sockaddr_storage *, socklen_t *);
