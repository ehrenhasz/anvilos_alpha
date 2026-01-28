


#ifndef _NET_SUNRPC_SOCKLIB_H_
#define _NET_SUNRPC_SOCKLIB_H_

int csum_partial_copy_to_xdr(struct xdr_buf *xdr, struct sk_buff *skb);
int xprt_sock_sendmsg(struct socket *sock, struct msghdr *msg,
		      struct xdr_buf *xdr, unsigned int base,
		      rpc_fraghdr marker, unsigned int *sent_p);

#endif 
