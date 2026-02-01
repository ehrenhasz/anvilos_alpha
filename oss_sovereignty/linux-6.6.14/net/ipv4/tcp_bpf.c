
 
	if (unlikely(!skb_queue_empty(&sk->sk_receive_queue))) {
		tcp_data_ready(sk);
		 
		if (unlikely(!skb_queue_empty(&sk->sk_receive_queue))) {
			copied = -EAGAIN;
			goto out;
		}
	}

msg_bytes_ready:
	copied = sk_msg_recvmsg(sk, psock, msg, len, flags);
	 
	if (copied == -EFAULT) {
		bool is_fin = is_next_msg_fin(psock);

		if (is_fin) {
			copied = 0;
			seq++;
			goto out;
		}
	}
	seq += copied;
	if (!copied) {
		long timeo;
		int data;

		if (sock_flag(sk, SOCK_DONE))
			goto out;

		if (sk->sk_err) {
			copied = sock_error(sk);
			goto out;
		}

		if (sk->sk_shutdown & RCV_SHUTDOWN)
			goto out;

		if (sk->sk_state == TCP_CLOSE) {
			copied = -ENOTCONN;
			goto out;
		}

		timeo = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);
		if (!timeo) {
			copied = -EAGAIN;
			goto out;
		}

		if (signal_pending(current)) {
			copied = sock_intr_errno(timeo);
			goto out;
		}

		data = tcp_msg_wait_data(sk, psock, timeo);
		if (data < 0) {
			copied = data;
			goto unlock;
		}
		if (data && !sk_psock_queue_empty(psock))
			goto msg_bytes_ready;
		copied = -EAGAIN;
	}
out:
	if (!peek)
		WRITE_ONCE(tcp->copied_seq, seq);
	tcp_rcv_space_adjust(sk);
	if (copied > 0)
		__tcp_cleanup_rbuf(sk, copied);

unlock:
	release_sock(sk);
	sk_psock_put(sk, psock);
	return copied;
}

static int tcp_bpf_recvmsg(struct sock *sk, struct msghdr *msg, size_t len,
			   int flags, int *addr_len)
{
	struct sk_psock *psock;
	int copied, ret;

	if (unlikely(flags & MSG_ERRQUEUE))
		return inet_recv_error(sk, msg, len, addr_len);

	if (!len)
		return 0;

	psock = sk_psock_get(sk);
	if (unlikely(!psock))
		return tcp_recvmsg(sk, msg, len, flags, addr_len);
	if (!skb_queue_empty(&sk->sk_receive_queue) &&
	    sk_psock_queue_empty(psock)) {
		sk_psock_put(sk, psock);
		return tcp_recvmsg(sk, msg, len, flags, addr_len);
	}
	lock_sock(sk);
msg_bytes_ready:
	copied = sk_msg_recvmsg(sk, psock, msg, len, flags);
	if (!copied) {
		long timeo;
		int data;

		timeo = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);
		data = tcp_msg_wait_data(sk, psock, timeo);
		if (data < 0) {
			ret = data;
			goto unlock;
		}
		if (data) {
			if (!sk_psock_queue_empty(psock))
				goto msg_bytes_ready;
			release_sock(sk);
			sk_psock_put(sk, psock);
			return tcp_recvmsg(sk, msg, len, flags, addr_len);
		}
		copied = -EAGAIN;
	}
	ret = copied;

unlock:
	release_sock(sk);
	sk_psock_put(sk, psock);
	return ret;
}

static int tcp_bpf_send_verdict(struct sock *sk, struct sk_psock *psock,
				struct sk_msg *msg, int *copied, int flags)
{
	bool cork = false, enospc = sk_msg_full(msg), redir_ingress;
	struct sock *sk_redir;
	u32 tosend, origsize, sent, delta = 0;
	u32 eval;
	int ret;

more_data:
	if (psock->eval == __SK_NONE) {
		 
		delta = msg->sg.size;
		psock->eval = sk_psock_msg_verdict(sk, psock, msg);
		delta -= msg->sg.size;
	}

	if (msg->cork_bytes &&
	    msg->cork_bytes > msg->sg.size && !enospc) {
		psock->cork_bytes = msg->cork_bytes - msg->sg.size;
		if (!psock->cork) {
			psock->cork = kzalloc(sizeof(*psock->cork),
					      GFP_ATOMIC | __GFP_NOWARN);
			if (!psock->cork)
				return -ENOMEM;
		}
		memcpy(psock->cork, msg, sizeof(*msg));
		return 0;
	}

	tosend = msg->sg.size;
	if (psock->apply_bytes && psock->apply_bytes < tosend)
		tosend = psock->apply_bytes;
	eval = __SK_NONE;

	switch (psock->eval) {
	case __SK_PASS:
		ret = tcp_bpf_push(sk, msg, tosend, flags, true);
		if (unlikely(ret)) {
			*copied -= sk_msg_free(sk, msg);
			break;
		}
		sk_msg_apply_bytes(psock, tosend);
		break;
	case __SK_REDIRECT:
		redir_ingress = psock->redir_ingress;
		sk_redir = psock->sk_redir;
		sk_msg_apply_bytes(psock, tosend);
		if (!psock->apply_bytes) {
			 
			eval = psock->eval;
			psock->eval = __SK_NONE;
			psock->sk_redir = NULL;
		}
		if (psock->cork) {
			cork = true;
			psock->cork = NULL;
		}
		sk_msg_return(sk, msg, tosend);
		release_sock(sk);

		origsize = msg->sg.size;
		ret = tcp_bpf_sendmsg_redir(sk_redir, redir_ingress,
					    msg, tosend, flags);
		sent = origsize - msg->sg.size;

		if (eval == __SK_REDIRECT)
			sock_put(sk_redir);

		lock_sock(sk);
		if (unlikely(ret < 0)) {
			int free = sk_msg_free_nocharge(sk, msg);

			if (!cork)
				*copied -= free;
		}
		if (cork) {
			sk_msg_free(sk, msg);
			kfree(msg);
			msg = NULL;
			ret = 0;
		}
		break;
	case __SK_DROP:
	default:
		sk_msg_free_partial(sk, msg, tosend);
		sk_msg_apply_bytes(psock, tosend);
		*copied -= (tosend + delta);
		return -EACCES;
	}

	if (likely(!ret)) {
		if (!psock->apply_bytes) {
			psock->eval =  __SK_NONE;
			if (psock->sk_redir) {
				sock_put(psock->sk_redir);
				psock->sk_redir = NULL;
			}
		}
		if (msg &&
		    msg->sg.data[msg->sg.start].page_link &&
		    msg->sg.data[msg->sg.start].length) {
			if (eval == __SK_REDIRECT)
				sk_mem_charge(sk, tosend - sent);
			goto more_data;
		}
	}
	return ret;
}

static int tcp_bpf_sendmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
	struct sk_msg tmp, *msg_tx = NULL;
	int copied = 0, err = 0;
	struct sk_psock *psock;
	long timeo;
	int flags;

	 
	flags = (msg->msg_flags & ~MSG_SENDPAGE_DECRYPTED);
	flags |= MSG_NO_SHARED_FRAGS;

	psock = sk_psock_get(sk);
	if (unlikely(!psock))
		return tcp_sendmsg(sk, msg, size);

	lock_sock(sk);
	timeo = sock_sndtimeo(sk, msg->msg_flags & MSG_DONTWAIT);
	while (msg_data_left(msg)) {
		bool enospc = false;
		u32 copy, osize;

		if (sk->sk_err) {
			err = -sk->sk_err;
			goto out_err;
		}

		copy = msg_data_left(msg);
		if (!sk_stream_memory_free(sk))
			goto wait_for_sndbuf;
		if (psock->cork) {
			msg_tx = psock->cork;
		} else {
			msg_tx = &tmp;
			sk_msg_init(msg_tx);
		}

		osize = msg_tx->sg.size;
		err = sk_msg_alloc(sk, msg_tx, msg_tx->sg.size + copy, msg_tx->sg.end - 1);
		if (err) {
			if (err != -ENOSPC)
				goto wait_for_memory;
			enospc = true;
			copy = msg_tx->sg.size - osize;
		}

		err = sk_msg_memcopy_from_iter(sk, &msg->msg_iter, msg_tx,
					       copy);
		if (err < 0) {
			sk_msg_trim(sk, msg_tx, osize);
			goto out_err;
		}

		copied += copy;
		if (psock->cork_bytes) {
			if (size > psock->cork_bytes)
				psock->cork_bytes = 0;
			else
				psock->cork_bytes -= size;
			if (psock->cork_bytes && !enospc)
				goto out_err;
			 
			psock->eval = __SK_NONE;
			psock->cork_bytes = 0;
		}

		err = tcp_bpf_send_verdict(sk, psock, msg_tx, &copied, flags);
		if (unlikely(err < 0))
			goto out_err;
		continue;
wait_for_sndbuf:
		set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
wait_for_memory:
		err = sk_stream_wait_memory(sk, &timeo);
		if (err) {
			if (msg_tx && msg_tx != psock->cork)
				sk_msg_free(sk, msg_tx);
			goto out_err;
		}
	}
out_err:
	if (err < 0)
		err = sk_stream_error(sk, msg->msg_flags, err);
	release_sock(sk);
	sk_psock_put(sk, psock);
	return copied ? copied : err;
}

enum {
	TCP_BPF_IPV4,
	TCP_BPF_IPV6,
	TCP_BPF_NUM_PROTS,
};

enum {
	TCP_BPF_BASE,
	TCP_BPF_TX,
	TCP_BPF_RX,
	TCP_BPF_TXRX,
	TCP_BPF_NUM_CFGS,
};

static struct proto *tcpv6_prot_saved __read_mostly;
static DEFINE_SPINLOCK(tcpv6_prot_lock);
static struct proto tcp_bpf_prots[TCP_BPF_NUM_PROTS][TCP_BPF_NUM_CFGS];

static void tcp_bpf_rebuild_protos(struct proto prot[TCP_BPF_NUM_CFGS],
				   struct proto *base)
{
	prot[TCP_BPF_BASE]			= *base;
	prot[TCP_BPF_BASE].destroy		= sock_map_destroy;
	prot[TCP_BPF_BASE].close		= sock_map_close;
	prot[TCP_BPF_BASE].recvmsg		= tcp_bpf_recvmsg;
	prot[TCP_BPF_BASE].sock_is_readable	= sk_msg_is_readable;

	prot[TCP_BPF_TX]			= prot[TCP_BPF_BASE];
	prot[TCP_BPF_TX].sendmsg		= tcp_bpf_sendmsg;

	prot[TCP_BPF_RX]			= prot[TCP_BPF_BASE];
	prot[TCP_BPF_RX].recvmsg		= tcp_bpf_recvmsg_parser;

	prot[TCP_BPF_TXRX]			= prot[TCP_BPF_TX];
	prot[TCP_BPF_TXRX].recvmsg		= tcp_bpf_recvmsg_parser;
}

static void tcp_bpf_check_v6_needs_rebuild(struct proto *ops)
{
	if (unlikely(ops != smp_load_acquire(&tcpv6_prot_saved))) {
		spin_lock_bh(&tcpv6_prot_lock);
		if (likely(ops != tcpv6_prot_saved)) {
			tcp_bpf_rebuild_protos(tcp_bpf_prots[TCP_BPF_IPV6], ops);
			smp_store_release(&tcpv6_prot_saved, ops);
		}
		spin_unlock_bh(&tcpv6_prot_lock);
	}
}

static int __init tcp_bpf_v4_build_proto(void)
{
	tcp_bpf_rebuild_protos(tcp_bpf_prots[TCP_BPF_IPV4], &tcp_prot);
	return 0;
}
late_initcall(tcp_bpf_v4_build_proto);

static int tcp_bpf_assert_proto_ops(struct proto *ops)
{
	 
	return ops->recvmsg  == tcp_recvmsg &&
	       ops->sendmsg  == tcp_sendmsg ? 0 : -ENOTSUPP;
}

int tcp_bpf_update_proto(struct sock *sk, struct sk_psock *psock, bool restore)
{
	int family = sk->sk_family == AF_INET6 ? TCP_BPF_IPV6 : TCP_BPF_IPV4;
	int config = psock->progs.msg_parser   ? TCP_BPF_TX   : TCP_BPF_BASE;

	if (psock->progs.stream_verdict || psock->progs.skb_verdict) {
		config = (config == TCP_BPF_TX) ? TCP_BPF_TXRX : TCP_BPF_RX;
	}

	if (restore) {
		if (inet_csk_has_ulp(sk)) {
			 
			WRITE_ONCE(sk->sk_prot->unhash, psock->saved_unhash);
			tcp_update_ulp(sk, psock->sk_proto, psock->saved_write_space);
		} else {
			sk->sk_write_space = psock->saved_write_space;
			 
			sock_replace_proto(sk, psock->sk_proto);
		}
		return 0;
	}

	if (sk->sk_family == AF_INET6) {
		if (tcp_bpf_assert_proto_ops(psock->sk_proto))
			return -EINVAL;

		tcp_bpf_check_v6_needs_rebuild(psock->sk_proto);
	}

	 
	sock_replace_proto(sk, &tcp_bpf_prots[family][config]);
	return 0;
}
EXPORT_SYMBOL_GPL(tcp_bpf_update_proto);

 
void tcp_bpf_clone(const struct sock *sk, struct sock *newsk)
{
	struct proto *prot = newsk->sk_prot;

	if (is_insidevar(prot, tcp_bpf_prots))
		newsk->sk_prot = sk->sk_prot_creator;
}
#endif  
