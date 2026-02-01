
 

#include <linux/module.h>
#include <linux/sched/signal.h>
#include <linux/net.h>
#include <linux/signal.h>
#include <linux/tcp.h>
#include <linux/wait.h>
#include <net/sock.h>

 
void sk_stream_write_space(struct sock *sk)
{
	struct socket *sock = sk->sk_socket;
	struct socket_wq *wq;

	if (__sk_stream_is_writeable(sk, 1) && sock) {
		clear_bit(SOCK_NOSPACE, &sock->flags);

		rcu_read_lock();
		wq = rcu_dereference(sk->sk_wq);
		if (skwq_has_sleeper(wq))
			wake_up_interruptible_poll(&wq->wait, EPOLLOUT |
						EPOLLWRNORM | EPOLLWRBAND);
		if (wq && wq->fasync_list && !(sk->sk_shutdown & SEND_SHUTDOWN))
			sock_wake_async(wq, SOCK_WAKE_SPACE, POLL_OUT);
		rcu_read_unlock();
	}
}

 
int sk_stream_wait_connect(struct sock *sk, long *timeo_p)
{
	DEFINE_WAIT_FUNC(wait, woken_wake_function);
	struct task_struct *tsk = current;
	int done;

	do {
		int err = sock_error(sk);
		if (err)
			return err;
		if ((1 << sk->sk_state) & ~(TCPF_SYN_SENT | TCPF_SYN_RECV))
			return -EPIPE;
		if (!*timeo_p)
			return -EAGAIN;
		if (signal_pending(tsk))
			return sock_intr_errno(*timeo_p);

		add_wait_queue(sk_sleep(sk), &wait);
		sk->sk_write_pending++;
		done = sk_wait_event(sk, timeo_p,
				     !READ_ONCE(sk->sk_err) &&
				     !((1 << READ_ONCE(sk->sk_state)) &
				       ~(TCPF_ESTABLISHED | TCPF_CLOSE_WAIT)), &wait);
		remove_wait_queue(sk_sleep(sk), &wait);
		sk->sk_write_pending--;
	} while (!done);
	return done < 0 ? done : 0;
}
EXPORT_SYMBOL(sk_stream_wait_connect);

 
static int sk_stream_closing(const struct sock *sk)
{
	return (1 << READ_ONCE(sk->sk_state)) &
	       (TCPF_FIN_WAIT1 | TCPF_CLOSING | TCPF_LAST_ACK);
}

void sk_stream_wait_close(struct sock *sk, long timeout)
{
	if (timeout) {
		DEFINE_WAIT_FUNC(wait, woken_wake_function);

		add_wait_queue(sk_sleep(sk), &wait);

		do {
			if (sk_wait_event(sk, &timeout, !sk_stream_closing(sk), &wait))
				break;
		} while (!signal_pending(current) && timeout);

		remove_wait_queue(sk_sleep(sk), &wait);
	}
}
EXPORT_SYMBOL(sk_stream_wait_close);

 
int sk_stream_wait_memory(struct sock *sk, long *timeo_p)
{
	int ret, err = 0;
	long vm_wait = 0;
	long current_timeo = *timeo_p;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	if (sk_stream_memory_free(sk))
		current_timeo = vm_wait = get_random_u32_below(HZ / 5) + 2;

	add_wait_queue(sk_sleep(sk), &wait);

	while (1) {
		sk_set_bit(SOCKWQ_ASYNC_NOSPACE, sk);

		if (sk->sk_err || (sk->sk_shutdown & SEND_SHUTDOWN))
			goto do_error;
		if (!*timeo_p)
			goto do_eagain;
		if (signal_pending(current))
			goto do_interrupted;
		sk_clear_bit(SOCKWQ_ASYNC_NOSPACE, sk);
		if (sk_stream_memory_free(sk) && !vm_wait)
			break;

		set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
		sk->sk_write_pending++;
		ret = sk_wait_event(sk, &current_timeo, READ_ONCE(sk->sk_err) ||
				    (READ_ONCE(sk->sk_shutdown) & SEND_SHUTDOWN) ||
				    (sk_stream_memory_free(sk) && !vm_wait),
				    &wait);
		sk->sk_write_pending--;
		if (ret < 0)
			goto do_error;

		if (vm_wait) {
			vm_wait -= current_timeo;
			current_timeo = *timeo_p;
			if (current_timeo != MAX_SCHEDULE_TIMEOUT &&
			    (current_timeo -= vm_wait) < 0)
				current_timeo = 0;
			vm_wait = 0;
		}
		*timeo_p = current_timeo;
	}
out:
	if (!sock_flag(sk, SOCK_DEAD))
		remove_wait_queue(sk_sleep(sk), &wait);
	return err;

do_error:
	err = -EPIPE;
	goto out;
do_eagain:
	 
	set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
	err = -EAGAIN;
	goto out;
do_interrupted:
	err = sock_intr_errno(*timeo_p);
	goto out;
}
EXPORT_SYMBOL(sk_stream_wait_memory);

int sk_stream_error(struct sock *sk, int flags, int err)
{
	if (err == -EPIPE)
		err = sock_error(sk) ? : -EPIPE;
	if (err == -EPIPE && !(flags & MSG_NOSIGNAL))
		send_sig(SIGPIPE, current, 0);
	return err;
}
EXPORT_SYMBOL(sk_stream_error);

void sk_stream_kill_queues(struct sock *sk)
{
	 
	__skb_queue_purge(&sk->sk_receive_queue);

	 
	skb_queue_purge(&sk->sk_error_queue);

	 
	WARN_ON_ONCE(!skb_queue_empty(&sk->sk_write_queue));

	 
	sk_mem_reclaim_final(sk);

	WARN_ON_ONCE(sk->sk_wmem_queued);

	 
}
EXPORT_SYMBOL(sk_stream_kill_queues);
