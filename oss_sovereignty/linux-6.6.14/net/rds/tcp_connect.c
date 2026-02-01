 
#include <linux/kernel.h>
#include <linux/in.h>
#include <net/tcp.h>

#include "rds.h"
#include "tcp.h"

void rds_tcp_state_change(struct sock *sk)
{
	void (*state_change)(struct sock *sk);
	struct rds_conn_path *cp;
	struct rds_tcp_connection *tc;

	read_lock_bh(&sk->sk_callback_lock);
	cp = sk->sk_user_data;
	if (!cp) {
		state_change = sk->sk_state_change;
		goto out;
	}
	tc = cp->cp_transport_data;
	state_change = tc->t_orig_state_change;

	rdsdebug("sock %p state_change to %d\n", tc->t_sock, sk->sk_state);

	switch (sk->sk_state) {
	 
	case TCP_SYN_SENT:
	case TCP_SYN_RECV:
		break;
	case TCP_ESTABLISHED:
		 
		if (rds_addr_cmp(&cp->cp_conn->c_laddr,
				 &cp->cp_conn->c_faddr) >= 0 &&
		    rds_conn_path_transition(cp, RDS_CONN_CONNECTING,
					     RDS_CONN_ERROR)) {
			rds_conn_path_drop(cp, false);
		} else {
			rds_connect_path_complete(cp, RDS_CONN_CONNECTING);
		}
		break;
	case TCP_CLOSE_WAIT:
	case TCP_CLOSE:
		rds_conn_path_drop(cp, false);
		break;
	default:
		break;
	}
out:
	read_unlock_bh(&sk->sk_callback_lock);
	state_change(sk);
}

int rds_tcp_conn_path_connect(struct rds_conn_path *cp)
{
	struct socket *sock = NULL;
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	struct sockaddr *addr;
	int addrlen;
	bool isv6;
	int ret;
	struct rds_connection *conn = cp->cp_conn;
	struct rds_tcp_connection *tc = cp->cp_transport_data;

	 
	if (cp->cp_index > 0 && cp->cp_conn->c_npaths < 2)
		return -EAGAIN;

	mutex_lock(&tc->t_conn_path_lock);

	if (rds_conn_path_up(cp)) {
		mutex_unlock(&tc->t_conn_path_lock);
		return 0;
	}
	if (ipv6_addr_v4mapped(&conn->c_laddr)) {
		ret = sock_create_kern(rds_conn_net(conn), PF_INET,
				       SOCK_STREAM, IPPROTO_TCP, &sock);
		isv6 = false;
	} else {
		ret = sock_create_kern(rds_conn_net(conn), PF_INET6,
				       SOCK_STREAM, IPPROTO_TCP, &sock);
		isv6 = true;
	}

	if (ret < 0)
		goto out;

	if (!rds_tcp_tune(sock)) {
		ret = -EINVAL;
		goto out;
	}

	if (isv6) {
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = conn->c_laddr;
		sin6.sin6_port = 0;
		sin6.sin6_flowinfo = 0;
		sin6.sin6_scope_id = conn->c_dev_if;
		addr = (struct sockaddr *)&sin6;
		addrlen = sizeof(sin6);
	} else {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = conn->c_laddr.s6_addr32[3];
		sin.sin_port = 0;
		addr = (struct sockaddr *)&sin;
		addrlen = sizeof(sin);
	}

	ret = kernel_bind(sock, addr, addrlen);
	if (ret) {
		rdsdebug("bind failed with %d at address %pI6c\n",
			 ret, &conn->c_laddr);
		goto out;
	}

	if (isv6) {
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = conn->c_faddr;
		sin6.sin6_port = htons(RDS_TCP_PORT);
		sin6.sin6_flowinfo = 0;
		sin6.sin6_scope_id = conn->c_dev_if;
		addr = (struct sockaddr *)&sin6;
		addrlen = sizeof(sin6);
	} else {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = conn->c_faddr.s6_addr32[3];
		sin.sin_port = htons(RDS_TCP_PORT);
		addr = (struct sockaddr *)&sin;
		addrlen = sizeof(sin);
	}

	 
	rds_tcp_set_callbacks(sock, cp);
	ret = kernel_connect(sock, addr, addrlen, O_NONBLOCK);

	rdsdebug("connect to address %pI6c returned %d\n", &conn->c_faddr, ret);
	if (ret == -EINPROGRESS)
		ret = 0;
	if (ret == 0) {
		rds_tcp_keepalive(sock);
		sock = NULL;
	} else {
		rds_tcp_restore_callbacks(sock, cp->cp_transport_data);
	}

out:
	mutex_unlock(&tc->t_conn_path_lock);
	if (sock)
		sock_release(sock);
	return ret;
}

 
void rds_tcp_conn_path_shutdown(struct rds_conn_path *cp)
{
	struct rds_tcp_connection *tc = cp->cp_transport_data;
	struct socket *sock = tc->t_sock;

	rdsdebug("shutting down conn %p tc %p sock %p\n",
		 cp->cp_conn, tc, sock);

	if (sock) {
		if (rds_destroy_pending(cp->cp_conn))
			sock_no_linger(sock->sk);
		sock->ops->shutdown(sock, RCV_SHUTDOWN | SEND_SHUTDOWN);
		lock_sock(sock->sk);
		rds_tcp_restore_callbacks(sock, tc);  

		release_sock(sock->sk);
		sock_release(sock);
	}

	if (tc->t_tinc) {
		rds_inc_put(&tc->t_tinc->ti_inc);
		tc->t_tinc = NULL;
	}
	tc->t_tinc_hdr_rem = sizeof(struct rds_header);
	tc->t_tinc_data_rem = 0;
}
