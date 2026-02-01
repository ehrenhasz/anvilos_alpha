 
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/in.h>
#include <net/tcp.h>
#include <trace/events/sock.h>

#include "rds.h"
#include "tcp.h"

void rds_tcp_keepalive(struct socket *sock)
{
	 
	int keepidle = 5;  
	int keepcnt = 5;  

	sock_set_keepalive(sock->sk);
	tcp_sock_set_keepcnt(sock->sk, keepcnt);
	tcp_sock_set_keepidle(sock->sk, keepidle);
	 
	tcp_sock_set_keepintvl(sock->sk, keepidle);
}

 
static
struct rds_tcp_connection *rds_tcp_accept_one_path(struct rds_connection *conn)
{
	int i;
	int npaths = max_t(int, 1, conn->c_npaths);

	 
	if (rds_addr_cmp(&conn->c_faddr, &conn->c_laddr) >= 0) {
		 
		if (npaths == 1)
			rds_conn_path_connect_if_down(&conn->c_path[0]);
		return NULL;
	}

	for (i = 0; i < npaths; i++) {
		struct rds_conn_path *cp = &conn->c_path[i];

		if (rds_conn_path_transition(cp, RDS_CONN_DOWN,
					     RDS_CONN_CONNECTING) ||
		    rds_conn_path_transition(cp, RDS_CONN_ERROR,
					     RDS_CONN_CONNECTING)) {
			return cp->cp_transport_data;
		}
	}
	return NULL;
}

int rds_tcp_accept_one(struct socket *sock)
{
	struct socket *new_sock = NULL;
	struct rds_connection *conn;
	int ret;
	struct inet_sock *inet;
	struct rds_tcp_connection *rs_tcp = NULL;
	int conn_state;
	struct rds_conn_path *cp;
	struct in6_addr *my_addr, *peer_addr;
#if !IS_ENABLED(CONFIG_IPV6)
	struct in6_addr saddr, daddr;
#endif
	int dev_if = 0;

	if (!sock)  
		return -ENETUNREACH;

	ret = sock_create_lite(sock->sk->sk_family,
			       sock->sk->sk_type, sock->sk->sk_protocol,
			       &new_sock);
	if (ret)
		goto out;

	ret = sock->ops->accept(sock, new_sock, O_NONBLOCK, true);
	if (ret < 0)
		goto out;

	 
	new_sock->ops = sock->ops;
	__module_get(new_sock->ops->owner);

	rds_tcp_keepalive(new_sock);
	if (!rds_tcp_tune(new_sock)) {
		ret = -EINVAL;
		goto out;
	}

	inet = inet_sk(new_sock->sk);

#if IS_ENABLED(CONFIG_IPV6)
	my_addr = &new_sock->sk->sk_v6_rcv_saddr;
	peer_addr = &new_sock->sk->sk_v6_daddr;
#else
	ipv6_addr_set_v4mapped(inet->inet_saddr, &saddr);
	ipv6_addr_set_v4mapped(inet->inet_daddr, &daddr);
	my_addr = &saddr;
	peer_addr = &daddr;
#endif
	rdsdebug("accepted family %d tcp %pI6c:%u -> %pI6c:%u\n",
		 sock->sk->sk_family,
		 my_addr, ntohs(inet->inet_sport),
		 peer_addr, ntohs(inet->inet_dport));

#if IS_ENABLED(CONFIG_IPV6)
	 
	if ((ipv6_addr_type(my_addr) & IPV6_ADDR_LINKLOCAL) &&
	    !(ipv6_addr_type(peer_addr) & IPV6_ADDR_LINKLOCAL)) {
		struct ipv6_pinfo *inet6;

		inet6 = inet6_sk(new_sock->sk);
		dev_if = inet6->mcast_oif;
	} else {
		dev_if = new_sock->sk->sk_bound_dev_if;
	}
#endif

	if (!rds_tcp_laddr_check(sock_net(sock->sk), peer_addr, dev_if)) {
		 
		ret = -EOPNOTSUPP;
		goto out;
	}

	conn = rds_conn_create(sock_net(sock->sk),
			       my_addr, peer_addr,
			       &rds_tcp_transport, 0, GFP_KERNEL, dev_if);

	if (IS_ERR(conn)) {
		ret = PTR_ERR(conn);
		goto out;
	}
	 
	rs_tcp = rds_tcp_accept_one_path(conn);
	if (!rs_tcp)
		goto rst_nsk;
	mutex_lock(&rs_tcp->t_conn_path_lock);
	cp = rs_tcp->t_cpath;
	conn_state = rds_conn_path_state(cp);
	WARN_ON(conn_state == RDS_CONN_UP);
	if (conn_state != RDS_CONN_CONNECTING && conn_state != RDS_CONN_ERROR)
		goto rst_nsk;
	if (rs_tcp->t_sock) {
		 
		rds_tcp_reset_callbacks(new_sock, cp);
		 
		rds_connect_path_complete(cp, RDS_CONN_RESETTING);
	} else {
		rds_tcp_set_callbacks(new_sock, cp);
		rds_connect_path_complete(cp, RDS_CONN_CONNECTING);
	}
	new_sock = NULL;
	ret = 0;
	if (conn->c_npaths == 0)
		rds_send_ping(cp->cp_conn, cp->cp_index);
	goto out;
rst_nsk:
	 
	sock_no_linger(new_sock->sk);
	kernel_sock_shutdown(new_sock, SHUT_RDWR);
	ret = 0;
out:
	if (rs_tcp)
		mutex_unlock(&rs_tcp->t_conn_path_lock);
	if (new_sock)
		sock_release(new_sock);
	return ret;
}

void rds_tcp_listen_data_ready(struct sock *sk)
{
	void (*ready)(struct sock *sk);

	trace_sk_data_ready(sk);
	rdsdebug("listen data ready sk %p\n", sk);

	read_lock_bh(&sk->sk_callback_lock);
	ready = sk->sk_user_data;
	if (!ready) {  
		ready = sk->sk_data_ready;
		goto out;
	}

	 
	if (sk->sk_state == TCP_LISTEN)
		rds_tcp_accept_work(sk);
	else
		ready = rds_tcp_listen_sock_def_readable(sock_net(sk));

out:
	read_unlock_bh(&sk->sk_callback_lock);
	if (ready)
		ready(sk);
}

struct socket *rds_tcp_listen_init(struct net *net, bool isv6)
{
	struct socket *sock = NULL;
	struct sockaddr_storage ss;
	struct sockaddr_in6 *sin6;
	struct sockaddr_in *sin;
	int addr_len;
	int ret;

	ret = sock_create_kern(net, isv6 ? PF_INET6 : PF_INET, SOCK_STREAM,
			       IPPROTO_TCP, &sock);
	if (ret < 0) {
		rdsdebug("could not create %s listener socket: %d\n",
			 isv6 ? "IPv6" : "IPv4", ret);
		goto out;
	}

	sock->sk->sk_reuse = SK_CAN_REUSE;
	tcp_sock_set_nodelay(sock->sk);

	write_lock_bh(&sock->sk->sk_callback_lock);
	sock->sk->sk_user_data = sock->sk->sk_data_ready;
	sock->sk->sk_data_ready = rds_tcp_listen_data_ready;
	write_unlock_bh(&sock->sk->sk_callback_lock);

	if (isv6) {
		sin6 = (struct sockaddr_in6 *)&ss;
		sin6->sin6_family = PF_INET6;
		sin6->sin6_addr = in6addr_any;
		sin6->sin6_port = (__force u16)htons(RDS_TCP_PORT);
		sin6->sin6_scope_id = 0;
		sin6->sin6_flowinfo = 0;
		addr_len = sizeof(*sin6);
	} else {
		sin = (struct sockaddr_in *)&ss;
		sin->sin_family = PF_INET;
		sin->sin_addr.s_addr = INADDR_ANY;
		sin->sin_port = (__force u16)htons(RDS_TCP_PORT);
		addr_len = sizeof(*sin);
	}

	ret = kernel_bind(sock, (struct sockaddr *)&ss, addr_len);
	if (ret < 0) {
		rdsdebug("could not bind %s listener socket: %d\n",
			 isv6 ? "IPv6" : "IPv4", ret);
		goto out;
	}

	ret = sock->ops->listen(sock, 64);
	if (ret < 0)
		goto out;

	return sock;
out:
	if (sock)
		sock_release(sock);
	return NULL;
}

void rds_tcp_listen_stop(struct socket *sock, struct work_struct *acceptor)
{
	struct sock *sk;

	if (!sock)
		return;

	sk = sock->sk;

	 
	lock_sock(sk);
	write_lock_bh(&sk->sk_callback_lock);
	if (sk->sk_user_data) {
		sk->sk_data_ready = sk->sk_user_data;
		sk->sk_user_data = NULL;
	}
	write_unlock_bh(&sk->sk_callback_lock);
	release_sock(sk);

	 
	flush_workqueue(rds_wq);
	flush_work(acceptor);
	sock_release(sock);
}
