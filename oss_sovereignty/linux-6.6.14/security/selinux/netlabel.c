
 

 

#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/gfp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/sock.h>
#include <net/netlabel.h>
#include <net/ip.h>
#include <net/ipv6.h>

#include "objsec.h"
#include "security.h"
#include "netlabel.h"

 
static int selinux_netlbl_sidlookup_cached(struct sk_buff *skb,
					   u16 family,
					   struct netlbl_lsm_secattr *secattr,
					   u32 *sid)
{
	int rc;

	rc = security_netlbl_secattr_to_sid(secattr, sid);
	if (rc == 0 &&
	    (secattr->flags & NETLBL_SECATTR_CACHEABLE) &&
	    (secattr->flags & NETLBL_SECATTR_CACHE))
		netlbl_cache_add(skb, family, secattr);

	return rc;
}

 
static struct netlbl_lsm_secattr *selinux_netlbl_sock_genattr(struct sock *sk)
{
	int rc;
	struct sk_security_struct *sksec = sk->sk_security;
	struct netlbl_lsm_secattr *secattr;

	if (sksec->nlbl_secattr != NULL)
		return sksec->nlbl_secattr;

	secattr = netlbl_secattr_alloc(GFP_ATOMIC);
	if (secattr == NULL)
		return NULL;
	rc = security_netlbl_sid_to_secattr(sksec->sid, secattr);
	if (rc != 0) {
		netlbl_secattr_free(secattr);
		return NULL;
	}
	sksec->nlbl_secattr = secattr;

	return secattr;
}

 
static struct netlbl_lsm_secattr *selinux_netlbl_sock_getattr(
							const struct sock *sk,
							u32 sid)
{
	struct sk_security_struct *sksec = sk->sk_security;
	struct netlbl_lsm_secattr *secattr = sksec->nlbl_secattr;

	if (secattr == NULL)
		return NULL;

	if ((secattr->flags & NETLBL_SECATTR_SECID) &&
	    (secattr->attr.secid == sid))
		return secattr;

	return NULL;
}

 
void selinux_netlbl_cache_invalidate(void)
{
	netlbl_cache_invalidate();
}

 
void selinux_netlbl_err(struct sk_buff *skb, u16 family, int error, int gateway)
{
	netlbl_skbuff_err(skb, family, error, gateway);
}

 
void selinux_netlbl_sk_security_free(struct sk_security_struct *sksec)
{
	if (!sksec->nlbl_secattr)
		return;

	netlbl_secattr_free(sksec->nlbl_secattr);
	sksec->nlbl_secattr = NULL;
	sksec->nlbl_state = NLBL_UNSET;
}

 
void selinux_netlbl_sk_security_reset(struct sk_security_struct *sksec)
{
	sksec->nlbl_state = NLBL_UNSET;
}

 
int selinux_netlbl_skbuff_getsid(struct sk_buff *skb,
				 u16 family,
				 u32 *type,
				 u32 *sid)
{
	int rc;
	struct netlbl_lsm_secattr secattr;

	if (!netlbl_enabled()) {
		*type = NETLBL_NLTYPE_NONE;
		*sid = SECSID_NULL;
		return 0;
	}

	netlbl_secattr_init(&secattr);
	rc = netlbl_skbuff_getattr(skb, family, &secattr);
	if (rc == 0 && secattr.flags != NETLBL_SECATTR_NONE)
		rc = selinux_netlbl_sidlookup_cached(skb, family,
						     &secattr, sid);
	else
		*sid = SECSID_NULL;
	*type = secattr.type;
	netlbl_secattr_destroy(&secattr);

	return rc;
}

 
int selinux_netlbl_skbuff_setsid(struct sk_buff *skb,
				 u16 family,
				 u32 sid)
{
	int rc;
	struct netlbl_lsm_secattr secattr_storage;
	struct netlbl_lsm_secattr *secattr = NULL;
	struct sock *sk;

	 
	sk = skb_to_full_sk(skb);
	if (sk != NULL) {
		struct sk_security_struct *sksec = sk->sk_security;

		if (sksec->nlbl_state != NLBL_REQSKB)
			return 0;
		secattr = selinux_netlbl_sock_getattr(sk, sid);
	}
	if (secattr == NULL) {
		secattr = &secattr_storage;
		netlbl_secattr_init(secattr);
		rc = security_netlbl_sid_to_secattr(sid, secattr);
		if (rc != 0)
			goto skbuff_setsid_return;
	}

	rc = netlbl_skbuff_setattr(skb, family, secattr);

skbuff_setsid_return:
	if (secattr == &secattr_storage)
		netlbl_secattr_destroy(secattr);
	return rc;
}

 
int selinux_netlbl_sctp_assoc_request(struct sctp_association *asoc,
				     struct sk_buff *skb)
{
	int rc;
	struct netlbl_lsm_secattr secattr;
	struct sk_security_struct *sksec = asoc->base.sk->sk_security;
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;

	if (asoc->base.sk->sk_family != PF_INET &&
	    asoc->base.sk->sk_family != PF_INET6)
		return 0;

	netlbl_secattr_init(&secattr);
	rc = security_netlbl_sid_to_secattr(asoc->secid, &secattr);
	if (rc != 0)
		goto assoc_request_return;

	 
	if (ip_hdr(skb)->version == 4) {
		addr4.sin_family = AF_INET;
		addr4.sin_addr.s_addr = ip_hdr(skb)->saddr;
		rc = netlbl_conn_setattr(asoc->base.sk, (void *)&addr4, &secattr);
	} else if (IS_ENABLED(CONFIG_IPV6) && ip_hdr(skb)->version == 6) {
		addr6.sin6_family = AF_INET6;
		addr6.sin6_addr = ipv6_hdr(skb)->saddr;
		rc = netlbl_conn_setattr(asoc->base.sk, (void *)&addr6, &secattr);
	} else {
		rc = -EAFNOSUPPORT;
	}

	if (rc == 0)
		sksec->nlbl_state = NLBL_LABELED;

assoc_request_return:
	netlbl_secattr_destroy(&secattr);
	return rc;
}

 
int selinux_netlbl_inet_conn_request(struct request_sock *req, u16 family)
{
	int rc;
	struct netlbl_lsm_secattr secattr;

	if (family != PF_INET && family != PF_INET6)
		return 0;

	netlbl_secattr_init(&secattr);
	rc = security_netlbl_sid_to_secattr(req->secid, &secattr);
	if (rc != 0)
		goto inet_conn_request_return;
	rc = netlbl_req_setattr(req, &secattr);
inet_conn_request_return:
	netlbl_secattr_destroy(&secattr);
	return rc;
}

 
void selinux_netlbl_inet_csk_clone(struct sock *sk, u16 family)
{
	struct sk_security_struct *sksec = sk->sk_security;

	if (family == PF_INET)
		sksec->nlbl_state = NLBL_LABELED;
	else
		sksec->nlbl_state = NLBL_UNSET;
}

 
void selinux_netlbl_sctp_sk_clone(struct sock *sk, struct sock *newsk)
{
	struct sk_security_struct *sksec = sk->sk_security;
	struct sk_security_struct *newsksec = newsk->sk_security;

	newsksec->nlbl_state = sksec->nlbl_state;
}

 
int selinux_netlbl_socket_post_create(struct sock *sk, u16 family)
{
	int rc;
	struct sk_security_struct *sksec = sk->sk_security;
	struct netlbl_lsm_secattr *secattr;

	if (family != PF_INET && family != PF_INET6)
		return 0;

	secattr = selinux_netlbl_sock_genattr(sk);
	if (secattr == NULL)
		return -ENOMEM;
	rc = netlbl_sock_setattr(sk, family, secattr);
	switch (rc) {
	case 0:
		sksec->nlbl_state = NLBL_LABELED;
		break;
	case -EDESTADDRREQ:
		sksec->nlbl_state = NLBL_REQSKB;
		rc = 0;
		break;
	}

	return rc;
}

 
int selinux_netlbl_sock_rcv_skb(struct sk_security_struct *sksec,
				struct sk_buff *skb,
				u16 family,
				struct common_audit_data *ad)
{
	int rc;
	u32 nlbl_sid;
	u32 perm;
	struct netlbl_lsm_secattr secattr;

	if (!netlbl_enabled())
		return 0;

	netlbl_secattr_init(&secattr);
	rc = netlbl_skbuff_getattr(skb, family, &secattr);
	if (rc == 0 && secattr.flags != NETLBL_SECATTR_NONE)
		rc = selinux_netlbl_sidlookup_cached(skb, family,
						     &secattr, &nlbl_sid);
	else
		nlbl_sid = SECINITSID_UNLABELED;
	netlbl_secattr_destroy(&secattr);
	if (rc != 0)
		return rc;

	switch (sksec->sclass) {
	case SECCLASS_UDP_SOCKET:
		perm = UDP_SOCKET__RECVFROM;
		break;
	case SECCLASS_TCP_SOCKET:
		perm = TCP_SOCKET__RECVFROM;
		break;
	default:
		perm = RAWIP_SOCKET__RECVFROM;
	}

	rc = avc_has_perm(sksec->sid, nlbl_sid, sksec->sclass, perm, ad);
	if (rc == 0)
		return 0;

	if (nlbl_sid != SECINITSID_UNLABELED)
		netlbl_skbuff_err(skb, family, rc, 0);
	return rc;
}

 
static inline int selinux_netlbl_option(int level, int optname)
{
	return (level == IPPROTO_IP && optname == IP_OPTIONS) ||
		(level == IPPROTO_IPV6 && optname == IPV6_HOPOPTS);
}

 
int selinux_netlbl_socket_setsockopt(struct socket *sock,
				     int level,
				     int optname)
{
	int rc = 0;
	struct sock *sk = sock->sk;
	struct sk_security_struct *sksec = sk->sk_security;
	struct netlbl_lsm_secattr secattr;

	if (selinux_netlbl_option(level, optname) &&
	    (sksec->nlbl_state == NLBL_LABELED ||
	     sksec->nlbl_state == NLBL_CONNLABELED)) {
		netlbl_secattr_init(&secattr);
		lock_sock(sk);
		 
		rc = netlbl_sock_getattr(sk, &secattr);
		release_sock(sk);
		if (rc == 0)
			rc = -EACCES;
		else if (rc == -ENOMSG)
			rc = 0;
		netlbl_secattr_destroy(&secattr);
	}

	return rc;
}

 
static int selinux_netlbl_socket_connect_helper(struct sock *sk,
						struct sockaddr *addr)
{
	int rc;
	struct sk_security_struct *sksec = sk->sk_security;
	struct netlbl_lsm_secattr *secattr;

	 
	if (addr->sa_family == AF_UNSPEC) {
		netlbl_sock_delattr(sk);
		sksec->nlbl_state = NLBL_REQSKB;
		rc = 0;
		return rc;
	}
	secattr = selinux_netlbl_sock_genattr(sk);
	if (secattr == NULL) {
		rc = -ENOMEM;
		return rc;
	}
	rc = netlbl_conn_setattr(sk, addr, secattr);
	if (rc == 0)
		sksec->nlbl_state = NLBL_CONNLABELED;

	return rc;
}

 
int selinux_netlbl_socket_connect_locked(struct sock *sk,
					 struct sockaddr *addr)
{
	struct sk_security_struct *sksec = sk->sk_security;

	if (sksec->nlbl_state != NLBL_REQSKB &&
	    sksec->nlbl_state != NLBL_CONNLABELED)
		return 0;

	return selinux_netlbl_socket_connect_helper(sk, addr);
}

 
int selinux_netlbl_socket_connect(struct sock *sk, struct sockaddr *addr)
{
	int rc;

	lock_sock(sk);
	rc = selinux_netlbl_socket_connect_locked(sk, addr);
	release_sock(sk);

	return rc;
}
