
 

#define pr_fmt(fmt) "IPv4: " fmt

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/capability.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/netfilter_ipv4.h>
#include <linux/random.h>
#include <linux/slab.h>

#include <linux/uaccess.h>

#include <linux/inet.h>
#include <linux/igmp.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <net/checksum.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/arp.h>
#include <net/route.h>
#include <net/ip_fib.h>
#include <net/inet_connection_sock.h>
#include <net/gro.h>
#include <net/gso.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/udplite.h>
#include <net/ping.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/raw.h>
#include <net/icmp.h>
#include <net/inet_common.h>
#include <net/ip_tunnels.h>
#include <net/xfrm.h>
#include <net/net_namespace.h>
#include <net/secure_seq.h>
#ifdef CONFIG_IP_MROUTE
#include <linux/mroute.h>
#endif
#include <net/l3mdev.h>
#include <net/compat.h>

#include <trace/events/sock.h>

 
static struct list_head inetsw[SOCK_MAX];
static DEFINE_SPINLOCK(inetsw_lock);

 

void inet_sock_destruct(struct sock *sk)
{
	struct inet_sock *inet = inet_sk(sk);

	__skb_queue_purge(&sk->sk_receive_queue);
	__skb_queue_purge(&sk->sk_error_queue);

	sk_mem_reclaim_final(sk);

	if (sk->sk_type == SOCK_STREAM && sk->sk_state != TCP_CLOSE) {
		pr_err("Attempt to release TCP socket in state %d %p\n",
		       sk->sk_state, sk);
		return;
	}
	if (!sock_flag(sk, SOCK_DEAD)) {
		pr_err("Attempt to release alive inet socket %p\n", sk);
		return;
	}

	WARN_ON_ONCE(atomic_read(&sk->sk_rmem_alloc));
	WARN_ON_ONCE(refcount_read(&sk->sk_wmem_alloc));
	WARN_ON_ONCE(sk->sk_wmem_queued);
	WARN_ON_ONCE(sk_forward_alloc_get(sk));

	kfree(rcu_dereference_protected(inet->inet_opt, 1));
	dst_release(rcu_dereference_protected(sk->sk_dst_cache, 1));
	dst_release(rcu_dereference_protected(sk->sk_rx_dst, 1));
}
EXPORT_SYMBOL(inet_sock_destruct);

 

 

static int inet_autobind(struct sock *sk)
{
	struct inet_sock *inet;
	 
	lock_sock(sk);
	inet = inet_sk(sk);
	if (!inet->inet_num) {
		if (sk->sk_prot->get_port(sk, 0)) {
			release_sock(sk);
			return -EAGAIN;
		}
		inet->inet_sport = htons(inet->inet_num);
	}
	release_sock(sk);
	return 0;
}

int __inet_listen_sk(struct sock *sk, int backlog)
{
	unsigned char old_state = sk->sk_state;
	int err, tcp_fastopen;

	if (!((1 << old_state) & (TCPF_CLOSE | TCPF_LISTEN)))
		return -EINVAL;

	WRITE_ONCE(sk->sk_max_ack_backlog, backlog);
	 
	if (old_state != TCP_LISTEN) {
		 
		tcp_fastopen = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_fastopen);
		if ((tcp_fastopen & TFO_SERVER_WO_SOCKOPT1) &&
		    (tcp_fastopen & TFO_SERVER_ENABLE) &&
		    !inet_csk(sk)->icsk_accept_queue.fastopenq.max_qlen) {
			fastopen_queue_tune(sk, backlog);
			tcp_fastopen_init_key_once(sock_net(sk));
		}

		err = inet_csk_listen_start(sk);
		if (err)
			return err;

		tcp_call_bpf(sk, BPF_SOCK_OPS_TCP_LISTEN_CB, 0, NULL);
	}
	return 0;
}

 
int inet_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;
	int err = -EINVAL;

	lock_sock(sk);

	if (sock->state != SS_UNCONNECTED || sock->type != SOCK_STREAM)
		goto out;

	err = __inet_listen_sk(sk, backlog);

out:
	release_sock(sk);
	return err;
}
EXPORT_SYMBOL(inet_listen);

 

static int inet_create(struct net *net, struct socket *sock, int protocol,
		       int kern)
{
	struct sock *sk;
	struct inet_protosw *answer;
	struct inet_sock *inet;
	struct proto *answer_prot;
	unsigned char answer_flags;
	int try_loading_module = 0;
	int err;

	if (protocol < 0 || protocol >= IPPROTO_MAX)
		return -EINVAL;

	sock->state = SS_UNCONNECTED;

	 
lookup_protocol:
	err = -ESOCKTNOSUPPORT;
	rcu_read_lock();
	list_for_each_entry_rcu(answer, &inetsw[sock->type], list) {

		err = 0;
		 
		if (protocol == answer->protocol) {
			if (protocol != IPPROTO_IP)
				break;
		} else {
			 
			if (IPPROTO_IP == protocol) {
				protocol = answer->protocol;
				break;
			}
			if (IPPROTO_IP == answer->protocol)
				break;
		}
		err = -EPROTONOSUPPORT;
	}

	if (unlikely(err)) {
		if (try_loading_module < 2) {
			rcu_read_unlock();
			 
			if (++try_loading_module == 1)
				request_module("net-pf-%d-proto-%d-type-%d",
					       PF_INET, protocol, sock->type);
			 
			else
				request_module("net-pf-%d-proto-%d",
					       PF_INET, protocol);
			goto lookup_protocol;
		} else
			goto out_rcu_unlock;
	}

	err = -EPERM;
	if (sock->type == SOCK_RAW && !kern &&
	    !ns_capable(net->user_ns, CAP_NET_RAW))
		goto out_rcu_unlock;

	sock->ops = answer->ops;
	answer_prot = answer->prot;
	answer_flags = answer->flags;
	rcu_read_unlock();

	WARN_ON(!answer_prot->slab);

	err = -ENOMEM;
	sk = sk_alloc(net, PF_INET, GFP_KERNEL, answer_prot, kern);
	if (!sk)
		goto out;

	err = 0;
	if (INET_PROTOSW_REUSE & answer_flags)
		sk->sk_reuse = SK_CAN_REUSE;

	inet = inet_sk(sk);
	inet_assign_bit(IS_ICSK, sk, INET_PROTOSW_ICSK & answer_flags);

	inet_clear_bit(NODEFRAG, sk);

	if (SOCK_RAW == sock->type) {
		inet->inet_num = protocol;
		if (IPPROTO_RAW == protocol)
			inet_set_bit(HDRINCL, sk);
	}

	if (READ_ONCE(net->ipv4.sysctl_ip_no_pmtu_disc))
		inet->pmtudisc = IP_PMTUDISC_DONT;
	else
		inet->pmtudisc = IP_PMTUDISC_WANT;

	atomic_set(&inet->inet_id, 0);

	sock_init_data(sock, sk);

	sk->sk_destruct	   = inet_sock_destruct;
	sk->sk_protocol	   = protocol;
	sk->sk_backlog_rcv = sk->sk_prot->backlog_rcv;
	sk->sk_txrehash = READ_ONCE(net->core.sysctl_txrehash);

	inet->uc_ttl	= -1;
	inet_set_bit(MC_LOOP, sk);
	inet->mc_ttl	= 1;
	inet_set_bit(MC_ALL, sk);
	inet->mc_index	= 0;
	inet->mc_list	= NULL;
	inet->rcv_tos	= 0;

	if (inet->inet_num) {
		 
		inet->inet_sport = htons(inet->inet_num);
		 
		err = sk->sk_prot->hash(sk);
		if (err) {
			sk_common_release(sk);
			goto out;
		}
	}

	if (sk->sk_prot->init) {
		err = sk->sk_prot->init(sk);
		if (err) {
			sk_common_release(sk);
			goto out;
		}
	}

	if (!kern) {
		err = BPF_CGROUP_RUN_PROG_INET_SOCK(sk);
		if (err) {
			sk_common_release(sk);
			goto out;
		}
	}
out:
	return err;
out_rcu_unlock:
	rcu_read_unlock();
	goto out;
}


 
int inet_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk) {
		long timeout;

		if (!sk->sk_kern_sock)
			BPF_CGROUP_RUN_PROG_INET_SOCK_RELEASE(sk);

		 
		ip_mc_drop_socket(sk);

		 
		timeout = 0;
		if (sock_flag(sk, SOCK_LINGER) &&
		    !(current->flags & PF_EXITING))
			timeout = sk->sk_lingertime;
		sk->sk_prot->close(sk, timeout);
		sock->sk = NULL;
	}
	return 0;
}
EXPORT_SYMBOL(inet_release);

int inet_bind_sk(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	u32 flags = BIND_WITH_LOCK;
	int err;

	 
	if (sk->sk_prot->bind) {
		return sk->sk_prot->bind(sk, uaddr, addr_len);
	}
	if (addr_len < sizeof(struct sockaddr_in))
		return -EINVAL;

	 
	err = BPF_CGROUP_RUN_PROG_INET_BIND_LOCK(sk, uaddr,
						 CGROUP_INET4_BIND, &flags);
	if (err)
		return err;

	return __inet_bind(sk, uaddr, addr_len, flags);
}

int inet_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	return inet_bind_sk(sock->sk, uaddr, addr_len);
}
EXPORT_SYMBOL(inet_bind);

int __inet_bind(struct sock *sk, struct sockaddr *uaddr, int addr_len,
		u32 flags)
{
	struct sockaddr_in *addr = (struct sockaddr_in *)uaddr;
	struct inet_sock *inet = inet_sk(sk);
	struct net *net = sock_net(sk);
	unsigned short snum;
	int chk_addr_ret;
	u32 tb_id = RT_TABLE_LOCAL;
	int err;

	if (addr->sin_family != AF_INET) {
		 
		err = -EAFNOSUPPORT;
		if (addr->sin_family != AF_UNSPEC ||
		    addr->sin_addr.s_addr != htonl(INADDR_ANY))
			goto out;
	}

	tb_id = l3mdev_fib_table_by_index(net, sk->sk_bound_dev_if) ? : tb_id;
	chk_addr_ret = inet_addr_type_table(net, addr->sin_addr.s_addr, tb_id);

	 
	err = -EADDRNOTAVAIL;
	if (!inet_addr_valid_or_nonlocal(net, inet, addr->sin_addr.s_addr,
	                                 chk_addr_ret))
		goto out;

	snum = ntohs(addr->sin_port);
	err = -EACCES;
	if (!(flags & BIND_NO_CAP_NET_BIND_SERVICE) &&
	    snum && inet_port_requires_bind_service(net, snum) &&
	    !ns_capable(net->user_ns, CAP_NET_BIND_SERVICE))
		goto out;

	 
	if (flags & BIND_WITH_LOCK)
		lock_sock(sk);

	 
	err = -EINVAL;
	if (sk->sk_state != TCP_CLOSE || inet->inet_num)
		goto out_release_sock;

	inet->inet_rcv_saddr = inet->inet_saddr = addr->sin_addr.s_addr;
	if (chk_addr_ret == RTN_MULTICAST || chk_addr_ret == RTN_BROADCAST)
		inet->inet_saddr = 0;   

	 
	if (snum || !(inet_test_bit(BIND_ADDRESS_NO_PORT, sk) ||
		      (flags & BIND_FORCE_ADDRESS_NO_PORT))) {
		err = sk->sk_prot->get_port(sk, snum);
		if (err) {
			inet->inet_saddr = inet->inet_rcv_saddr = 0;
			goto out_release_sock;
		}
		if (!(flags & BIND_FROM_BPF)) {
			err = BPF_CGROUP_RUN_PROG_INET4_POST_BIND(sk);
			if (err) {
				inet->inet_saddr = inet->inet_rcv_saddr = 0;
				if (sk->sk_prot->put_port)
					sk->sk_prot->put_port(sk);
				goto out_release_sock;
			}
		}
	}

	if (inet->inet_rcv_saddr)
		sk->sk_userlocks |= SOCK_BINDADDR_LOCK;
	if (snum)
		sk->sk_userlocks |= SOCK_BINDPORT_LOCK;
	inet->inet_sport = htons(inet->inet_num);
	inet->inet_daddr = 0;
	inet->inet_dport = 0;
	sk_dst_reset(sk);
	err = 0;
out_release_sock:
	if (flags & BIND_WITH_LOCK)
		release_sock(sk);
out:
	return err;
}

int inet_dgram_connect(struct socket *sock, struct sockaddr *uaddr,
		       int addr_len, int flags)
{
	struct sock *sk = sock->sk;
	const struct proto *prot;
	int err;

	if (addr_len < sizeof(uaddr->sa_family))
		return -EINVAL;

	 
	prot = READ_ONCE(sk->sk_prot);

	if (uaddr->sa_family == AF_UNSPEC)
		return prot->disconnect(sk, flags);

	if (BPF_CGROUP_PRE_CONNECT_ENABLED(sk)) {
		err = prot->pre_connect(sk, uaddr, addr_len);
		if (err)
			return err;
	}

	if (data_race(!inet_sk(sk)->inet_num) && inet_autobind(sk))
		return -EAGAIN;
	return prot->connect(sk, uaddr, addr_len);
}
EXPORT_SYMBOL(inet_dgram_connect);

static long inet_wait_for_connect(struct sock *sk, long timeo, int writebias)
{
	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	add_wait_queue(sk_sleep(sk), &wait);
	sk->sk_write_pending += writebias;

	 
	while ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV)) {
		release_sock(sk);
		timeo = wait_woken(&wait, TASK_INTERRUPTIBLE, timeo);
		lock_sock(sk);
		if (signal_pending(current) || !timeo)
			break;
	}
	remove_wait_queue(sk_sleep(sk), &wait);
	sk->sk_write_pending -= writebias;
	return timeo;
}

 
int __inet_stream_connect(struct socket *sock, struct sockaddr *uaddr,
			  int addr_len, int flags, int is_sendmsg)
{
	struct sock *sk = sock->sk;
	int err;
	long timeo;

	 
	if (uaddr) {
		if (addr_len < sizeof(uaddr->sa_family))
			return -EINVAL;

		if (uaddr->sa_family == AF_UNSPEC) {
			sk->sk_disconnects++;
			err = sk->sk_prot->disconnect(sk, flags);
			sock->state = err ? SS_DISCONNECTING : SS_UNCONNECTED;
			goto out;
		}
	}

	switch (sock->state) {
	default:
		err = -EINVAL;
		goto out;
	case SS_CONNECTED:
		err = -EISCONN;
		goto out;
	case SS_CONNECTING:
		if (inet_test_bit(DEFER_CONNECT, sk))
			err = is_sendmsg ? -EINPROGRESS : -EISCONN;
		else
			err = -EALREADY;
		 
		break;
	case SS_UNCONNECTED:
		err = -EISCONN;
		if (sk->sk_state != TCP_CLOSE)
			goto out;

		if (BPF_CGROUP_PRE_CONNECT_ENABLED(sk)) {
			err = sk->sk_prot->pre_connect(sk, uaddr, addr_len);
			if (err)
				goto out;
		}

		err = sk->sk_prot->connect(sk, uaddr, addr_len);
		if (err < 0)
			goto out;

		sock->state = SS_CONNECTING;

		if (!err && inet_test_bit(DEFER_CONNECT, sk))
			goto out;

		 
		err = -EINPROGRESS;
		break;
	}

	timeo = sock_sndtimeo(sk, flags & O_NONBLOCK);

	if ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV)) {
		int writebias = (sk->sk_protocol == IPPROTO_TCP) &&
				tcp_sk(sk)->fastopen_req &&
				tcp_sk(sk)->fastopen_req->data ? 1 : 0;
		int dis = sk->sk_disconnects;

		 
		if (!timeo || !inet_wait_for_connect(sk, timeo, writebias))
			goto out;

		err = sock_intr_errno(timeo);
		if (signal_pending(current))
			goto out;

		if (dis != sk->sk_disconnects) {
			err = -EPIPE;
			goto out;
		}
	}

	 
	if (sk->sk_state == TCP_CLOSE)
		goto sock_error;

	 

	sock->state = SS_CONNECTED;
	err = 0;
out:
	return err;

sock_error:
	err = sock_error(sk) ? : -ECONNABORTED;
	sock->state = SS_UNCONNECTED;
	sk->sk_disconnects++;
	if (sk->sk_prot->disconnect(sk, flags))
		sock->state = SS_DISCONNECTING;
	goto out;
}
EXPORT_SYMBOL(__inet_stream_connect);

int inet_stream_connect(struct socket *sock, struct sockaddr *uaddr,
			int addr_len, int flags)
{
	int err;

	lock_sock(sock->sk);
	err = __inet_stream_connect(sock, uaddr, addr_len, flags, 0);
	release_sock(sock->sk);
	return err;
}
EXPORT_SYMBOL(inet_stream_connect);

void __inet_accept(struct socket *sock, struct socket *newsock, struct sock *newsk)
{
	sock_rps_record_flow(newsk);
	WARN_ON(!((1 << newsk->sk_state) &
		  (TCPF_ESTABLISHED | TCPF_SYN_RECV |
		  TCPF_CLOSE_WAIT | TCPF_CLOSE)));

	if (test_bit(SOCK_SUPPORT_ZC, &sock->flags))
		set_bit(SOCK_SUPPORT_ZC, &newsock->flags);
	sock_graft(newsk, newsock);

	newsock->state = SS_CONNECTED;
}

 

int inet_accept(struct socket *sock, struct socket *newsock, int flags,
		bool kern)
{
	struct sock *sk1 = sock->sk, *sk2;
	int err = -EINVAL;

	 
	sk2 = READ_ONCE(sk1->sk_prot)->accept(sk1, flags, &err, kern);
	if (!sk2)
		return err;

	lock_sock(sk2);
	__inet_accept(sock, newsock, sk2);
	release_sock(sk2);
	return 0;
}
EXPORT_SYMBOL(inet_accept);

 
int inet_getname(struct socket *sock, struct sockaddr *uaddr,
		 int peer)
{
	struct sock *sk		= sock->sk;
	struct inet_sock *inet	= inet_sk(sk);
	DECLARE_SOCKADDR(struct sockaddr_in *, sin, uaddr);

	sin->sin_family = AF_INET;
	lock_sock(sk);
	if (peer) {
		if (!inet->inet_dport ||
		    (((1 << sk->sk_state) & (TCPF_CLOSE | TCPF_SYN_SENT)) &&
		     peer == 1)) {
			release_sock(sk);
			return -ENOTCONN;
		}
		sin->sin_port = inet->inet_dport;
		sin->sin_addr.s_addr = inet->inet_daddr;
		BPF_CGROUP_RUN_SA_PROG(sk, (struct sockaddr *)sin,
				       CGROUP_INET4_GETPEERNAME);
	} else {
		__be32 addr = inet->inet_rcv_saddr;
		if (!addr)
			addr = inet->inet_saddr;
		sin->sin_port = inet->inet_sport;
		sin->sin_addr.s_addr = addr;
		BPF_CGROUP_RUN_SA_PROG(sk, (struct sockaddr *)sin,
				       CGROUP_INET4_GETSOCKNAME);
	}
	release_sock(sk);
	memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
	return sizeof(*sin);
}
EXPORT_SYMBOL(inet_getname);

int inet_send_prepare(struct sock *sk)
{
	sock_rps_record_flow(sk);

	 
	if (data_race(!inet_sk(sk)->inet_num) && !sk->sk_prot->no_autobind &&
	    inet_autobind(sk))
		return -EAGAIN;

	return 0;
}
EXPORT_SYMBOL_GPL(inet_send_prepare);

int inet_sendmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock *sk = sock->sk;

	if (unlikely(inet_send_prepare(sk)))
		return -EAGAIN;

	return INDIRECT_CALL_2(sk->sk_prot->sendmsg, tcp_sendmsg, udp_sendmsg,
			       sk, msg, size);
}
EXPORT_SYMBOL(inet_sendmsg);

void inet_splice_eof(struct socket *sock)
{
	const struct proto *prot;
	struct sock *sk = sock->sk;

	if (unlikely(inet_send_prepare(sk)))
		return;

	 
	prot = READ_ONCE(sk->sk_prot);
	if (prot->splice_eof)
		prot->splice_eof(sock);
}
EXPORT_SYMBOL_GPL(inet_splice_eof);

INDIRECT_CALLABLE_DECLARE(int udp_recvmsg(struct sock *, struct msghdr *,
					  size_t, int, int *));
int inet_recvmsg(struct socket *sock, struct msghdr *msg, size_t size,
		 int flags)
{
	struct sock *sk = sock->sk;
	int addr_len = 0;
	int err;

	if (likely(!(flags & MSG_ERRQUEUE)))
		sock_rps_record_flow(sk);

	err = INDIRECT_CALL_2(sk->sk_prot->recvmsg, tcp_recvmsg, udp_recvmsg,
			      sk, msg, size, flags, &addr_len);
	if (err >= 0)
		msg->msg_namelen = addr_len;
	return err;
}
EXPORT_SYMBOL(inet_recvmsg);

int inet_shutdown(struct socket *sock, int how)
{
	struct sock *sk = sock->sk;
	int err = 0;

	 
	how++;  
	if ((how & ~SHUTDOWN_MASK) || !how)	 
		return -EINVAL;

	lock_sock(sk);
	if (sock->state == SS_CONNECTING) {
		if ((1 << sk->sk_state) &
		    (TCPF_SYN_SENT | TCPF_SYN_RECV | TCPF_CLOSE))
			sock->state = SS_DISCONNECTING;
		else
			sock->state = SS_CONNECTED;
	}

	switch (sk->sk_state) {
	case TCP_CLOSE:
		err = -ENOTCONN;
		 
		fallthrough;
	default:
		WRITE_ONCE(sk->sk_shutdown, sk->sk_shutdown | how);
		if (sk->sk_prot->shutdown)
			sk->sk_prot->shutdown(sk, how);
		break;

	 
	case TCP_LISTEN:
		if (!(how & RCV_SHUTDOWN))
			break;
		fallthrough;
	case TCP_SYN_SENT:
		err = sk->sk_prot->disconnect(sk, O_NONBLOCK);
		sock->state = err ? SS_DISCONNECTING : SS_UNCONNECTED;
		break;
	}

	 
	sk->sk_state_change(sk);
	release_sock(sk);
	return err;
}
EXPORT_SYMBOL(inet_shutdown);

 

int inet_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;
	int err = 0;
	struct net *net = sock_net(sk);
	void __user *p = (void __user *)arg;
	struct ifreq ifr;
	struct rtentry rt;

	switch (cmd) {
	case SIOCADDRT:
	case SIOCDELRT:
		if (copy_from_user(&rt, p, sizeof(struct rtentry)))
			return -EFAULT;
		err = ip_rt_ioctl(net, cmd, &rt);
		break;
	case SIOCRTMSG:
		err = -EINVAL;
		break;
	case SIOCDARP:
	case SIOCGARP:
	case SIOCSARP:
		err = arp_ioctl(net, cmd, (void __user *)arg);
		break;
	case SIOCGIFADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFPFLAGS:
		if (get_user_ifreq(&ifr, NULL, p))
			return -EFAULT;
		err = devinet_ioctl(net, cmd, &ifr);
		if (!err && put_user_ifreq(&ifr, p))
			err = -EFAULT;
		break;

	case SIOCSIFADDR:
	case SIOCSIFBRDADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
	case SIOCSIFPFLAGS:
	case SIOCSIFFLAGS:
		if (get_user_ifreq(&ifr, NULL, p))
			return -EFAULT;
		err = devinet_ioctl(net, cmd, &ifr);
		break;
	default:
		if (sk->sk_prot->ioctl)
			err = sk_ioctl(sk, cmd, (void __user *)arg);
		else
			err = -ENOIOCTLCMD;
		break;
	}
	return err;
}
EXPORT_SYMBOL(inet_ioctl);

#ifdef CONFIG_COMPAT
static int inet_compat_routing_ioctl(struct sock *sk, unsigned int cmd,
		struct compat_rtentry __user *ur)
{
	compat_uptr_t rtdev;
	struct rtentry rt;

	if (copy_from_user(&rt.rt_dst, &ur->rt_dst,
			3 * sizeof(struct sockaddr)) ||
	    get_user(rt.rt_flags, &ur->rt_flags) ||
	    get_user(rt.rt_metric, &ur->rt_metric) ||
	    get_user(rt.rt_mtu, &ur->rt_mtu) ||
	    get_user(rt.rt_window, &ur->rt_window) ||
	    get_user(rt.rt_irtt, &ur->rt_irtt) ||
	    get_user(rtdev, &ur->rt_dev))
		return -EFAULT;

	rt.rt_dev = compat_ptr(rtdev);
	return ip_rt_ioctl(sock_net(sk), cmd, &rt);
}

static int inet_compat_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	void __user *argp = compat_ptr(arg);
	struct sock *sk = sock->sk;

	switch (cmd) {
	case SIOCADDRT:
	case SIOCDELRT:
		return inet_compat_routing_ioctl(sk, cmd, argp);
	default:
		if (!sk->sk_prot->compat_ioctl)
			return -ENOIOCTLCMD;
		return sk->sk_prot->compat_ioctl(sk, cmd, arg);
	}
}
#endif  

const struct proto_ops inet_stream_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,
	.bind		   = inet_bind,
	.connect	   = inet_stream_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = inet_accept,
	.getname	   = inet_getname,
	.poll		   = tcp_poll,
	.ioctl		   = inet_ioctl,
	.gettstamp	   = sock_gettstamp,
	.listen		   = inet_listen,
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = inet_recvmsg,
#ifdef CONFIG_MMU
	.mmap		   = tcp_mmap,
#endif
	.splice_eof	   = inet_splice_eof,
	.splice_read	   = tcp_splice_read,
	.read_sock	   = tcp_read_sock,
	.read_skb	   = tcp_read_skb,
	.sendmsg_locked    = tcp_sendmsg_locked,
	.peek_len	   = tcp_peek_len,
#ifdef CONFIG_COMPAT
	.compat_ioctl	   = inet_compat_ioctl,
#endif
	.set_rcvlowat	   = tcp_set_rcvlowat,
};
EXPORT_SYMBOL(inet_stream_ops);

const struct proto_ops inet_dgram_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,
	.bind		   = inet_bind,
	.connect	   = inet_dgram_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = sock_no_accept,
	.getname	   = inet_getname,
	.poll		   = udp_poll,
	.ioctl		   = inet_ioctl,
	.gettstamp	   = sock_gettstamp,
	.listen		   = sock_no_listen,
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.read_skb	   = udp_read_skb,
	.recvmsg	   = inet_recvmsg,
	.mmap		   = sock_no_mmap,
	.splice_eof	   = inet_splice_eof,
	.set_peek_off	   = sk_set_peek_off,
#ifdef CONFIG_COMPAT
	.compat_ioctl	   = inet_compat_ioctl,
#endif
};
EXPORT_SYMBOL(inet_dgram_ops);

 
static const struct proto_ops inet_sockraw_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,
	.bind		   = inet_bind,
	.connect	   = inet_dgram_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = sock_no_accept,
	.getname	   = inet_getname,
	.poll		   = datagram_poll,
	.ioctl		   = inet_ioctl,
	.gettstamp	   = sock_gettstamp,
	.listen		   = sock_no_listen,
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = inet_recvmsg,
	.mmap		   = sock_no_mmap,
	.splice_eof	   = inet_splice_eof,
#ifdef CONFIG_COMPAT
	.compat_ioctl	   = inet_compat_ioctl,
#endif
};

static const struct net_proto_family inet_family_ops = {
	.family = PF_INET,
	.create = inet_create,
	.owner	= THIS_MODULE,
};

 
static struct inet_protosw inetsw_array[] =
{
	{
		.type =       SOCK_STREAM,
		.protocol =   IPPROTO_TCP,
		.prot =       &tcp_prot,
		.ops =        &inet_stream_ops,
		.flags =      INET_PROTOSW_PERMANENT |
			      INET_PROTOSW_ICSK,
	},

	{
		.type =       SOCK_DGRAM,
		.protocol =   IPPROTO_UDP,
		.prot =       &udp_prot,
		.ops =        &inet_dgram_ops,
		.flags =      INET_PROTOSW_PERMANENT,
       },

       {
		.type =       SOCK_DGRAM,
		.protocol =   IPPROTO_ICMP,
		.prot =       &ping_prot,
		.ops =        &inet_sockraw_ops,
		.flags =      INET_PROTOSW_REUSE,
       },

       {
	       .type =       SOCK_RAW,
	       .protocol =   IPPROTO_IP,	 
	       .prot =       &raw_prot,
	       .ops =        &inet_sockraw_ops,
	       .flags =      INET_PROTOSW_REUSE,
       }
};

#define INETSW_ARRAY_LEN ARRAY_SIZE(inetsw_array)

void inet_register_protosw(struct inet_protosw *p)
{
	struct list_head *lh;
	struct inet_protosw *answer;
	int protocol = p->protocol;
	struct list_head *last_perm;

	spin_lock_bh(&inetsw_lock);

	if (p->type >= SOCK_MAX)
		goto out_illegal;

	 
	last_perm = &inetsw[p->type];
	list_for_each(lh, &inetsw[p->type]) {
		answer = list_entry(lh, struct inet_protosw, list);
		 
		if ((INET_PROTOSW_PERMANENT & answer->flags) == 0)
			break;
		if (protocol == answer->protocol)
			goto out_permanent;
		last_perm = lh;
	}

	 
	list_add_rcu(&p->list, last_perm);
out:
	spin_unlock_bh(&inetsw_lock);

	return;

out_permanent:
	pr_err("Attempt to override permanent protocol %d\n", protocol);
	goto out;

out_illegal:
	pr_err("Ignoring attempt to register invalid socket type %d\n",
	       p->type);
	goto out;
}
EXPORT_SYMBOL(inet_register_protosw);

void inet_unregister_protosw(struct inet_protosw *p)
{
	if (INET_PROTOSW_PERMANENT & p->flags) {
		pr_err("Attempt to unregister permanent protocol %d\n",
		       p->protocol);
	} else {
		spin_lock_bh(&inetsw_lock);
		list_del_rcu(&p->list);
		spin_unlock_bh(&inetsw_lock);

		synchronize_net();
	}
}
EXPORT_SYMBOL(inet_unregister_protosw);

static int inet_sk_reselect_saddr(struct sock *sk)
{
	struct inet_sock *inet = inet_sk(sk);
	__be32 old_saddr = inet->inet_saddr;
	__be32 daddr = inet->inet_daddr;
	struct flowi4 *fl4;
	struct rtable *rt;
	__be32 new_saddr;
	struct ip_options_rcu *inet_opt;
	int err;

	inet_opt = rcu_dereference_protected(inet->inet_opt,
					     lockdep_sock_is_held(sk));
	if (inet_opt && inet_opt->opt.srr)
		daddr = inet_opt->opt.faddr;

	 
	fl4 = &inet->cork.fl.u.ip4;
	rt = ip_route_connect(fl4, daddr, 0, sk->sk_bound_dev_if,
			      sk->sk_protocol, inet->inet_sport,
			      inet->inet_dport, sk);
	if (IS_ERR(rt))
		return PTR_ERR(rt);

	new_saddr = fl4->saddr;

	if (new_saddr == old_saddr) {
		sk_setup_caps(sk, &rt->dst);
		return 0;
	}

	err = inet_bhash2_update_saddr(sk, &new_saddr, AF_INET);
	if (err) {
		ip_rt_put(rt);
		return err;
	}

	sk_setup_caps(sk, &rt->dst);

	if (READ_ONCE(sock_net(sk)->ipv4.sysctl_ip_dynaddr) > 1) {
		pr_info("%s(): shifting inet->saddr from %pI4 to %pI4\n",
			__func__, &old_saddr, &new_saddr);
	}

	 
	return __sk_prot_rehash(sk);
}

int inet_sk_rebuild_header(struct sock *sk)
{
	struct inet_sock *inet = inet_sk(sk);
	struct rtable *rt = (struct rtable *)__sk_dst_check(sk, 0);
	__be32 daddr;
	struct ip_options_rcu *inet_opt;
	struct flowi4 *fl4;
	int err;

	 
	if (rt)
		return 0;

	 
	rcu_read_lock();
	inet_opt = rcu_dereference(inet->inet_opt);
	daddr = inet->inet_daddr;
	if (inet_opt && inet_opt->opt.srr)
		daddr = inet_opt->opt.faddr;
	rcu_read_unlock();
	fl4 = &inet->cork.fl.u.ip4;
	rt = ip_route_output_ports(sock_net(sk), fl4, sk, daddr, inet->inet_saddr,
				   inet->inet_dport, inet->inet_sport,
				   sk->sk_protocol, RT_CONN_FLAGS(sk),
				   sk->sk_bound_dev_if);
	if (!IS_ERR(rt)) {
		err = 0;
		sk_setup_caps(sk, &rt->dst);
	} else {
		err = PTR_ERR(rt);

		 
		sk->sk_route_caps = 0;
		 
		if (!READ_ONCE(sock_net(sk)->ipv4.sysctl_ip_dynaddr) ||
		    sk->sk_state != TCP_SYN_SENT ||
		    (sk->sk_userlocks & SOCK_BINDADDR_LOCK) ||
		    (err = inet_sk_reselect_saddr(sk)) != 0)
			WRITE_ONCE(sk->sk_err_soft, -err);
	}

	return err;
}
EXPORT_SYMBOL(inet_sk_rebuild_header);

void inet_sk_set_state(struct sock *sk, int state)
{
	trace_inet_sock_set_state(sk, sk->sk_state, state);
	sk->sk_state = state;
}
EXPORT_SYMBOL(inet_sk_set_state);

void inet_sk_state_store(struct sock *sk, int newstate)
{
	trace_inet_sock_set_state(sk, sk->sk_state, newstate);
	smp_store_release(&sk->sk_state, newstate);
}

struct sk_buff *inet_gso_segment(struct sk_buff *skb,
				 netdev_features_t features)
{
	bool udpfrag = false, fixedid = false, gso_partial, encap;
	struct sk_buff *segs = ERR_PTR(-EINVAL);
	const struct net_offload *ops;
	unsigned int offset = 0;
	struct iphdr *iph;
	int proto, tot_len;
	int nhoff;
	int ihl;
	int id;

	skb_reset_network_header(skb);
	nhoff = skb_network_header(skb) - skb_mac_header(skb);
	if (unlikely(!pskb_may_pull(skb, sizeof(*iph))))
		goto out;

	iph = ip_hdr(skb);
	ihl = iph->ihl * 4;
	if (ihl < sizeof(*iph))
		goto out;

	id = ntohs(iph->id);
	proto = iph->protocol;

	 
	if (unlikely(!pskb_may_pull(skb, ihl)))
		goto out;
	__skb_pull(skb, ihl);

	encap = SKB_GSO_CB(skb)->encap_level > 0;
	if (encap)
		features &= skb->dev->hw_enc_features;
	SKB_GSO_CB(skb)->encap_level += ihl;

	skb_reset_transport_header(skb);

	segs = ERR_PTR(-EPROTONOSUPPORT);

	if (!skb->encapsulation || encap) {
		udpfrag = !!(skb_shinfo(skb)->gso_type & SKB_GSO_UDP);
		fixedid = !!(skb_shinfo(skb)->gso_type & SKB_GSO_TCP_FIXEDID);

		 
		if (fixedid && !(ip_hdr(skb)->frag_off & htons(IP_DF)))
			goto out;
	}

	ops = rcu_dereference(inet_offloads[proto]);
	if (likely(ops && ops->callbacks.gso_segment)) {
		segs = ops->callbacks.gso_segment(skb, features);
		if (!segs)
			skb->network_header = skb_mac_header(skb) + nhoff - skb->head;
	}

	if (IS_ERR_OR_NULL(segs))
		goto out;

	gso_partial = !!(skb_shinfo(segs)->gso_type & SKB_GSO_PARTIAL);

	skb = segs;
	do {
		iph = (struct iphdr *)(skb_mac_header(skb) + nhoff);
		if (udpfrag) {
			iph->frag_off = htons(offset >> 3);
			if (skb->next)
				iph->frag_off |= htons(IP_MF);
			offset += skb->len - nhoff - ihl;
			tot_len = skb->len - nhoff;
		} else if (skb_is_gso(skb)) {
			if (!fixedid) {
				iph->id = htons(id);
				id += skb_shinfo(skb)->gso_segs;
			}

			if (gso_partial)
				tot_len = skb_shinfo(skb)->gso_size +
					  SKB_GSO_CB(skb)->data_offset +
					  skb->head - (unsigned char *)iph;
			else
				tot_len = skb->len - nhoff;
		} else {
			if (!fixedid)
				iph->id = htons(id++);
			tot_len = skb->len - nhoff;
		}
		iph->tot_len = htons(tot_len);
		ip_send_check(iph);
		if (encap)
			skb_reset_inner_headers(skb);
		skb->network_header = (u8 *)iph - skb->head;
		skb_reset_mac_len(skb);
	} while ((skb = skb->next));

out:
	return segs;
}

static struct sk_buff *ipip_gso_segment(struct sk_buff *skb,
					netdev_features_t features)
{
	if (!(skb_shinfo(skb)->gso_type & SKB_GSO_IPXIP4))
		return ERR_PTR(-EINVAL);

	return inet_gso_segment(skb, features);
}

struct sk_buff *inet_gro_receive(struct list_head *head, struct sk_buff *skb)
{
	const struct net_offload *ops;
	struct sk_buff *pp = NULL;
	const struct iphdr *iph;
	struct sk_buff *p;
	unsigned int hlen;
	unsigned int off;
	unsigned int id;
	int flush = 1;
	int proto;

	off = skb_gro_offset(skb);
	hlen = off + sizeof(*iph);
	iph = skb_gro_header(skb, hlen, off);
	if (unlikely(!iph))
		goto out;

	proto = iph->protocol;

	ops = rcu_dereference(inet_offloads[proto]);
	if (!ops || !ops->callbacks.gro_receive)
		goto out;

	if (*(u8 *)iph != 0x45)
		goto out;

	if (ip_is_fragment(iph))
		goto out;

	if (unlikely(ip_fast_csum((u8 *)iph, 5)))
		goto out;

	NAPI_GRO_CB(skb)->proto = proto;
	id = ntohl(*(__be32 *)&iph->id);
	flush = (u16)((ntohl(*(__be32 *)iph) ^ skb_gro_len(skb)) | (id & ~IP_DF));
	id >>= 16;

	list_for_each_entry(p, head, list) {
		struct iphdr *iph2;
		u16 flush_id;

		if (!NAPI_GRO_CB(p)->same_flow)
			continue;

		iph2 = (struct iphdr *)(p->data + off);
		 
		if ((iph->protocol ^ iph2->protocol) |
		    ((__force u32)iph->saddr ^ (__force u32)iph2->saddr) |
		    ((__force u32)iph->daddr ^ (__force u32)iph2->daddr)) {
			NAPI_GRO_CB(p)->same_flow = 0;
			continue;
		}

		 
		NAPI_GRO_CB(p)->flush |=
			(iph->ttl ^ iph2->ttl) |
			(iph->tos ^ iph2->tos) |
			((iph->frag_off ^ iph2->frag_off) & htons(IP_DF));

		NAPI_GRO_CB(p)->flush |= flush;

		 
		flush_id = (u16)(id - ntohs(iph2->id));

		 
		if (!NAPI_GRO_CB(p)->is_atomic ||
		    !(iph->frag_off & htons(IP_DF))) {
			flush_id ^= NAPI_GRO_CB(p)->count;
			flush_id = flush_id ? 0xFFFF : 0;
		}

		 
		if (NAPI_GRO_CB(skb)->is_atomic)
			NAPI_GRO_CB(p)->flush_id = flush_id;
		else
			NAPI_GRO_CB(p)->flush_id |= flush_id;
	}

	NAPI_GRO_CB(skb)->is_atomic = !!(iph->frag_off & htons(IP_DF));
	NAPI_GRO_CB(skb)->flush |= flush;
	skb_set_network_header(skb, off);
	 

	 
	skb_gro_pull(skb, sizeof(*iph));
	skb_set_transport_header(skb, skb_gro_offset(skb));

	pp = indirect_call_gro_receive(tcp4_gro_receive, udp4_gro_receive,
				       ops->callbacks.gro_receive, head, skb);

out:
	skb_gro_flush_final(skb, pp, flush);

	return pp;
}

static struct sk_buff *ipip_gro_receive(struct list_head *head,
					struct sk_buff *skb)
{
	if (NAPI_GRO_CB(skb)->encap_mark) {
		NAPI_GRO_CB(skb)->flush = 1;
		return NULL;
	}

	NAPI_GRO_CB(skb)->encap_mark = 1;

	return inet_gro_receive(head, skb);
}

#define SECONDS_PER_DAY	86400

 
__be32 inet_current_timestamp(void)
{
	u32 secs;
	u32 msecs;
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);

	 
	(void)div_u64_rem(ts.tv_sec, SECONDS_PER_DAY, &secs);
	 
	msecs = secs * MSEC_PER_SEC;
	 
	msecs += (u32)ts.tv_nsec / NSEC_PER_MSEC;

	 
	return htonl(msecs);
}
EXPORT_SYMBOL(inet_current_timestamp);

int inet_recv_error(struct sock *sk, struct msghdr *msg, int len, int *addr_len)
{
	if (sk->sk_family == AF_INET)
		return ip_recv_error(sk, msg, len, addr_len);
#if IS_ENABLED(CONFIG_IPV6)
	if (sk->sk_family == AF_INET6)
		return pingv6_ops.ipv6_recv_error(sk, msg, len, addr_len);
#endif
	return -EINVAL;
}
EXPORT_SYMBOL(inet_recv_error);

int inet_gro_complete(struct sk_buff *skb, int nhoff)
{
	struct iphdr *iph = (struct iphdr *)(skb->data + nhoff);
	const struct net_offload *ops;
	__be16 totlen = iph->tot_len;
	int proto = iph->protocol;
	int err = -ENOSYS;

	if (skb->encapsulation) {
		skb_set_inner_protocol(skb, cpu_to_be16(ETH_P_IP));
		skb_set_inner_network_header(skb, nhoff);
	}

	iph_set_totlen(iph, skb->len - nhoff);
	csum_replace2(&iph->check, totlen, iph->tot_len);

	ops = rcu_dereference(inet_offloads[proto]);
	if (WARN_ON(!ops || !ops->callbacks.gro_complete))
		goto out;

	 
	err = INDIRECT_CALL_2(ops->callbacks.gro_complete,
			      tcp4_gro_complete, udp4_gro_complete,
			      skb, nhoff + sizeof(*iph));

out:
	return err;
}

static int ipip_gro_complete(struct sk_buff *skb, int nhoff)
{
	skb->encapsulation = 1;
	skb_shinfo(skb)->gso_type |= SKB_GSO_IPXIP4;
	return inet_gro_complete(skb, nhoff);
}

int inet_ctl_sock_create(struct sock **sk, unsigned short family,
			 unsigned short type, unsigned char protocol,
			 struct net *net)
{
	struct socket *sock;
	int rc = sock_create_kern(net, family, type, protocol, &sock);

	if (rc == 0) {
		*sk = sock->sk;
		(*sk)->sk_allocation = GFP_ATOMIC;
		(*sk)->sk_use_task_frag = false;
		 
		(*sk)->sk_prot->unhash(*sk);
	}
	return rc;
}
EXPORT_SYMBOL_GPL(inet_ctl_sock_create);

unsigned long snmp_fold_field(void __percpu *mib, int offt)
{
	unsigned long res = 0;
	int i;

	for_each_possible_cpu(i)
		res += snmp_get_cpu_field(mib, i, offt);
	return res;
}
EXPORT_SYMBOL_GPL(snmp_fold_field);

#if BITS_PER_LONG==32

u64 snmp_get_cpu_field64(void __percpu *mib, int cpu, int offt,
			 size_t syncp_offset)
{
	void *bhptr;
	struct u64_stats_sync *syncp;
	u64 v;
	unsigned int start;

	bhptr = per_cpu_ptr(mib, cpu);
	syncp = (struct u64_stats_sync *)(bhptr + syncp_offset);
	do {
		start = u64_stats_fetch_begin(syncp);
		v = *(((u64 *)bhptr) + offt);
	} while (u64_stats_fetch_retry(syncp, start));

	return v;
}
EXPORT_SYMBOL_GPL(snmp_get_cpu_field64);

u64 snmp_fold_field64(void __percpu *mib, int offt, size_t syncp_offset)
{
	u64 res = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		res += snmp_get_cpu_field64(mib, cpu, offt, syncp_offset);
	}
	return res;
}
EXPORT_SYMBOL_GPL(snmp_fold_field64);
#endif

#ifdef CONFIG_IP_MULTICAST
static const struct net_protocol igmp_protocol = {
	.handler =	igmp_rcv,
};
#endif

static const struct net_protocol tcp_protocol = {
	.handler	=	tcp_v4_rcv,
	.err_handler	=	tcp_v4_err,
	.no_policy	=	1,
	.icmp_strict_tag_validation = 1,
};

static const struct net_protocol udp_protocol = {
	.handler =	udp_rcv,
	.err_handler =	udp_err,
	.no_policy =	1,
};

static const struct net_protocol icmp_protocol = {
	.handler =	icmp_rcv,
	.err_handler =	icmp_err,
	.no_policy =	1,
};

static __net_init int ipv4_mib_init_net(struct net *net)
{
	int i;

	net->mib.tcp_statistics = alloc_percpu(struct tcp_mib);
	if (!net->mib.tcp_statistics)
		goto err_tcp_mib;
	net->mib.ip_statistics = alloc_percpu(struct ipstats_mib);
	if (!net->mib.ip_statistics)
		goto err_ip_mib;

	for_each_possible_cpu(i) {
		struct ipstats_mib *af_inet_stats;
		af_inet_stats = per_cpu_ptr(net->mib.ip_statistics, i);
		u64_stats_init(&af_inet_stats->syncp);
	}

	net->mib.net_statistics = alloc_percpu(struct linux_mib);
	if (!net->mib.net_statistics)
		goto err_net_mib;
	net->mib.udp_statistics = alloc_percpu(struct udp_mib);
	if (!net->mib.udp_statistics)
		goto err_udp_mib;
	net->mib.udplite_statistics = alloc_percpu(struct udp_mib);
	if (!net->mib.udplite_statistics)
		goto err_udplite_mib;
	net->mib.icmp_statistics = alloc_percpu(struct icmp_mib);
	if (!net->mib.icmp_statistics)
		goto err_icmp_mib;
	net->mib.icmpmsg_statistics = kzalloc(sizeof(struct icmpmsg_mib),
					      GFP_KERNEL);
	if (!net->mib.icmpmsg_statistics)
		goto err_icmpmsg_mib;

	tcp_mib_init(net);
	return 0;

err_icmpmsg_mib:
	free_percpu(net->mib.icmp_statistics);
err_icmp_mib:
	free_percpu(net->mib.udplite_statistics);
err_udplite_mib:
	free_percpu(net->mib.udp_statistics);
err_udp_mib:
	free_percpu(net->mib.net_statistics);
err_net_mib:
	free_percpu(net->mib.ip_statistics);
err_ip_mib:
	free_percpu(net->mib.tcp_statistics);
err_tcp_mib:
	return -ENOMEM;
}

static __net_exit void ipv4_mib_exit_net(struct net *net)
{
	kfree(net->mib.icmpmsg_statistics);
	free_percpu(net->mib.icmp_statistics);
	free_percpu(net->mib.udplite_statistics);
	free_percpu(net->mib.udp_statistics);
	free_percpu(net->mib.net_statistics);
	free_percpu(net->mib.ip_statistics);
	free_percpu(net->mib.tcp_statistics);
#ifdef CONFIG_MPTCP
	 
	free_percpu(net->mib.mptcp_statistics);
#endif
}

static __net_initdata struct pernet_operations ipv4_mib_ops = {
	.init = ipv4_mib_init_net,
	.exit = ipv4_mib_exit_net,
};

static int __init init_ipv4_mibs(void)
{
	return register_pernet_subsys(&ipv4_mib_ops);
}

static __net_init int inet_init_net(struct net *net)
{
	 
	seqlock_init(&net->ipv4.ip_local_ports.lock);
	net->ipv4.ip_local_ports.range[0] =  32768;
	net->ipv4.ip_local_ports.range[1] =  60999;

	seqlock_init(&net->ipv4.ping_group_range.lock);
	 
	net->ipv4.ping_group_range.range[0] = make_kgid(&init_user_ns, 1);
	net->ipv4.ping_group_range.range[1] = make_kgid(&init_user_ns, 0);

	 
	net->ipv4.sysctl_ip_default_ttl = IPDEFTTL;
	net->ipv4.sysctl_ip_fwd_update_priority = 1;
	net->ipv4.sysctl_ip_dynaddr = 0;
	net->ipv4.sysctl_ip_early_demux = 1;
	net->ipv4.sysctl_udp_early_demux = 1;
	net->ipv4.sysctl_tcp_early_demux = 1;
	net->ipv4.sysctl_nexthop_compat_mode = 1;
#ifdef CONFIG_SYSCTL
	net->ipv4.sysctl_ip_prot_sock = PROT_SOCK;
#endif

	 
	net->ipv4.sysctl_igmp_max_memberships = 20;
	net->ipv4.sysctl_igmp_max_msf = 10;
	 
	net->ipv4.sysctl_igmp_llm_reports = 1;
	net->ipv4.sysctl_igmp_qrv = 2;

	net->ipv4.sysctl_fib_notify_on_flag_change = 0;

	return 0;
}

static __net_initdata struct pernet_operations af_inet_ops = {
	.init = inet_init_net,
};

static int __init init_inet_pernet_ops(void)
{
	return register_pernet_subsys(&af_inet_ops);
}

static int ipv4_proc_init(void);

 

static struct packet_offload ip_packet_offload __read_mostly = {
	.type = cpu_to_be16(ETH_P_IP),
	.callbacks = {
		.gso_segment = inet_gso_segment,
		.gro_receive = inet_gro_receive,
		.gro_complete = inet_gro_complete,
	},
};

static const struct net_offload ipip_offload = {
	.callbacks = {
		.gso_segment	= ipip_gso_segment,
		.gro_receive	= ipip_gro_receive,
		.gro_complete	= ipip_gro_complete,
	},
};

static int __init ipip_offload_init(void)
{
	return inet_add_offload(&ipip_offload, IPPROTO_IPIP);
}

static int __init ipv4_offload_init(void)
{
	 
	if (udpv4_offload_init() < 0)
		pr_crit("%s: Cannot add UDP protocol offload\n", __func__);
	if (tcpv4_offload_init() < 0)
		pr_crit("%s: Cannot add TCP protocol offload\n", __func__);
	if (ipip_offload_init() < 0)
		pr_crit("%s: Cannot add IPIP protocol offload\n", __func__);

	dev_add_offload(&ip_packet_offload);
	return 0;
}

fs_initcall(ipv4_offload_init);

static struct packet_type ip_packet_type __read_mostly = {
	.type = cpu_to_be16(ETH_P_IP),
	.func = ip_rcv,
	.list_func = ip_list_rcv,
};

static int __init inet_init(void)
{
	struct inet_protosw *q;
	struct list_head *r;
	int rc;

	sock_skb_cb_check_size(sizeof(struct inet_skb_parm));

	raw_hashinfo_init(&raw_v4_hashinfo);

	rc = proto_register(&tcp_prot, 1);
	if (rc)
		goto out;

	rc = proto_register(&udp_prot, 1);
	if (rc)
		goto out_unregister_tcp_proto;

	rc = proto_register(&raw_prot, 1);
	if (rc)
		goto out_unregister_udp_proto;

	rc = proto_register(&ping_prot, 1);
	if (rc)
		goto out_unregister_raw_proto;

	 

	(void)sock_register(&inet_family_ops);

#ifdef CONFIG_SYSCTL
	ip_static_sysctl_init();
#endif

	 

	if (inet_add_protocol(&icmp_protocol, IPPROTO_ICMP) < 0)
		pr_crit("%s: Cannot add ICMP protocol\n", __func__);
	if (inet_add_protocol(&udp_protocol, IPPROTO_UDP) < 0)
		pr_crit("%s: Cannot add UDP protocol\n", __func__);
	if (inet_add_protocol(&tcp_protocol, IPPROTO_TCP) < 0)
		pr_crit("%s: Cannot add TCP protocol\n", __func__);
#ifdef CONFIG_IP_MULTICAST
	if (inet_add_protocol(&igmp_protocol, IPPROTO_IGMP) < 0)
		pr_crit("%s: Cannot add IGMP protocol\n", __func__);
#endif

	 
	for (r = &inetsw[0]; r < &inetsw[SOCK_MAX]; ++r)
		INIT_LIST_HEAD(r);

	for (q = inetsw_array; q < &inetsw_array[INETSW_ARRAY_LEN]; ++q)
		inet_register_protosw(q);

	 

	arp_init();

	 

	ip_init();

	 
	if (init_ipv4_mibs())
		panic("%s: Cannot init ipv4 mibs\n", __func__);

	 
	tcp_init();

	 
	udp_init();

	 
	udplite4_register();

	raw_init();

	ping_init();

	 

	if (icmp_init() < 0)
		panic("Failed to create the ICMP control socket.\n");

	 
#if defined(CONFIG_IP_MROUTE)
	if (ip_mr_init())
		pr_crit("%s: Cannot init ipv4 mroute\n", __func__);
#endif

	if (init_inet_pernet_ops())
		pr_crit("%s: Cannot init ipv4 inet pernet ops\n", __func__);

	ipv4_proc_init();

	ipfrag_init();

	dev_add_pack(&ip_packet_type);

	ip_tunnel_core_init();

	rc = 0;
out:
	return rc;
out_unregister_raw_proto:
	proto_unregister(&raw_prot);
out_unregister_udp_proto:
	proto_unregister(&udp_prot);
out_unregister_tcp_proto:
	proto_unregister(&tcp_prot);
	goto out;
}

fs_initcall(inet_init);

 

#ifdef CONFIG_PROC_FS
static int __init ipv4_proc_init(void)
{
	int rc = 0;

	if (raw_proc_init())
		goto out_raw;
	if (tcp4_proc_init())
		goto out_tcp;
	if (udp4_proc_init())
		goto out_udp;
	if (ping_proc_init())
		goto out_ping;
	if (ip_misc_proc_init())
		goto out_misc;
out:
	return rc;
out_misc:
	ping_proc_exit();
out_ping:
	udp4_proc_exit();
out_udp:
	tcp4_proc_exit();
out_tcp:
	raw_proc_exit();
out_raw:
	rc = -ENOMEM;
	goto out;
}

#else  
static int __init ipv4_proc_init(void)
{
	return 0;
}
#endif  
