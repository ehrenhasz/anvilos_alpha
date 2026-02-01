
 

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <net/tcp.h>

static DEFINE_SPINLOCK(tcp_ulp_list_lock);
static LIST_HEAD(tcp_ulp_list);

 
static struct tcp_ulp_ops *tcp_ulp_find(const char *name)
{
	struct tcp_ulp_ops *e;

	list_for_each_entry_rcu(e, &tcp_ulp_list, list,
				lockdep_is_held(&tcp_ulp_list_lock)) {
		if (strcmp(e->name, name) == 0)
			return e;
	}

	return NULL;
}

static const struct tcp_ulp_ops *__tcp_ulp_find_autoload(const char *name)
{
	const struct tcp_ulp_ops *ulp = NULL;

	rcu_read_lock();
	ulp = tcp_ulp_find(name);

#ifdef CONFIG_MODULES
	if (!ulp && capable(CAP_NET_ADMIN)) {
		rcu_read_unlock();
		request_module("tcp-ulp-%s", name);
		rcu_read_lock();
		ulp = tcp_ulp_find(name);
	}
#endif
	if (!ulp || !try_module_get(ulp->owner))
		ulp = NULL;

	rcu_read_unlock();
	return ulp;
}

 
int tcp_register_ulp(struct tcp_ulp_ops *ulp)
{
	int ret = 0;

	spin_lock(&tcp_ulp_list_lock);
	if (tcp_ulp_find(ulp->name))
		ret = -EEXIST;
	else
		list_add_tail_rcu(&ulp->list, &tcp_ulp_list);
	spin_unlock(&tcp_ulp_list_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(tcp_register_ulp);

void tcp_unregister_ulp(struct tcp_ulp_ops *ulp)
{
	spin_lock(&tcp_ulp_list_lock);
	list_del_rcu(&ulp->list);
	spin_unlock(&tcp_ulp_list_lock);

	synchronize_rcu();
}
EXPORT_SYMBOL_GPL(tcp_unregister_ulp);

 
void tcp_get_available_ulp(char *buf, size_t maxlen)
{
	struct tcp_ulp_ops *ulp_ops;
	size_t offs = 0;

	*buf = '\0';
	rcu_read_lock();
	list_for_each_entry_rcu(ulp_ops, &tcp_ulp_list, list) {
		offs += snprintf(buf + offs, maxlen - offs,
				 "%s%s",
				 offs == 0 ? "" : " ", ulp_ops->name);

		if (WARN_ON_ONCE(offs >= maxlen))
			break;
	}
	rcu_read_unlock();
}

void tcp_update_ulp(struct sock *sk, struct proto *proto,
		    void (*write_space)(struct sock *sk))
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	if (icsk->icsk_ulp_ops->update)
		icsk->icsk_ulp_ops->update(sk, proto, write_space);
}

void tcp_cleanup_ulp(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	 
	if (!icsk->icsk_ulp_ops)
		return;

	if (icsk->icsk_ulp_ops->release)
		icsk->icsk_ulp_ops->release(sk);
	module_put(icsk->icsk_ulp_ops->owner);

	icsk->icsk_ulp_ops = NULL;
}

static int __tcp_set_ulp(struct sock *sk, const struct tcp_ulp_ops *ulp_ops)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	int err;

	err = -EEXIST;
	if (icsk->icsk_ulp_ops)
		goto out_err;

	if (sk->sk_socket)
		clear_bit(SOCK_SUPPORT_ZC, &sk->sk_socket->flags);

	err = -ENOTCONN;
	if (!ulp_ops->clone && sk->sk_state == TCP_LISTEN)
		goto out_err;

	err = ulp_ops->init(sk);
	if (err)
		goto out_err;

	icsk->icsk_ulp_ops = ulp_ops;
	return 0;
out_err:
	module_put(ulp_ops->owner);
	return err;
}

int tcp_set_ulp(struct sock *sk, const char *name)
{
	const struct tcp_ulp_ops *ulp_ops;

	sock_owned_by_me(sk);

	ulp_ops = __tcp_ulp_find_autoload(name);
	if (!ulp_ops)
		return -ENOENT;

	return __tcp_set_ulp(sk, ulp_ops);
}
