
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/uio.h>
#include <linux/net.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/can.h>
#include <linux/can/core.h>
#include <linux/can/dev.h>  
#include <linux/can/skb.h>
#include <linux/can/raw.h>
#include <net/sock.h>
#include <net/net_namespace.h>

MODULE_DESCRIPTION("PF_CAN raw protocol");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Urs Thuermann <urs.thuermann@volkswagen.de>");
MODULE_ALIAS("can-proto-1");

#define RAW_MIN_NAMELEN CAN_REQUIRED_SIZE(struct sockaddr_can, can_ifindex)

#define MASK_ALL 0

 

struct uniqframe {
	int skbcnt;
	const struct sk_buff *skb;
	unsigned int join_rx_count;
};

struct raw_sock {
	struct sock sk;
	int bound;
	int ifindex;
	struct net_device *dev;
	netdevice_tracker dev_tracker;
	struct list_head notifier;
	int loopback;
	int recv_own_msgs;
	int fd_frames;
	int xl_frames;
	int join_filters;
	int count;                  
	struct can_filter dfilter;  
	struct can_filter *filter;  
	can_err_mask_t err_mask;
	struct uniqframe __percpu *uniq;
};

static LIST_HEAD(raw_notifier_list);
static DEFINE_SPINLOCK(raw_notifier_lock);
static struct raw_sock *raw_busy_notifier;

 
static inline unsigned int *raw_flags(struct sk_buff *skb)
{
	sock_skb_cb_check_size(sizeof(struct sockaddr_can) +
			       sizeof(unsigned int));

	 
	return (unsigned int *)(&((struct sockaddr_can *)skb->cb)[1]);
}

static inline struct raw_sock *raw_sk(const struct sock *sk)
{
	return (struct raw_sock *)sk;
}

static void raw_rcv(struct sk_buff *oskb, void *data)
{
	struct sock *sk = (struct sock *)data;
	struct raw_sock *ro = raw_sk(sk);
	struct sockaddr_can *addr;
	struct sk_buff *skb;
	unsigned int *pflags;

	 
	if (!ro->recv_own_msgs && oskb->sk == sk)
		return;

	 
	if ((!ro->fd_frames && can_is_canfd_skb(oskb)) ||
	    (!ro->xl_frames && can_is_canxl_skb(oskb)))
		return;

	 
	if (this_cpu_ptr(ro->uniq)->skb == oskb &&
	    this_cpu_ptr(ro->uniq)->skbcnt == can_skb_prv(oskb)->skbcnt) {
		if (!ro->join_filters)
			return;

		this_cpu_inc(ro->uniq->join_rx_count);
		 
		if (this_cpu_ptr(ro->uniq)->join_rx_count < ro->count)
			return;
	} else {
		this_cpu_ptr(ro->uniq)->skb = oskb;
		this_cpu_ptr(ro->uniq)->skbcnt = can_skb_prv(oskb)->skbcnt;
		this_cpu_ptr(ro->uniq)->join_rx_count = 1;
		 
		if (ro->join_filters && ro->count > 1)
			return;
	}

	 
	skb = skb_clone(oskb, GFP_ATOMIC);
	if (!skb)
		return;

	 

	sock_skb_cb_check_size(sizeof(struct sockaddr_can));
	addr = (struct sockaddr_can *)skb->cb;
	memset(addr, 0, sizeof(*addr));
	addr->can_family = AF_CAN;
	addr->can_ifindex = skb->dev->ifindex;

	 
	pflags = raw_flags(skb);
	*pflags = 0;
	if (oskb->sk)
		*pflags |= MSG_DONTROUTE;
	if (oskb->sk == sk)
		*pflags |= MSG_CONFIRM;

	if (sock_queue_rcv_skb(sk, skb) < 0)
		kfree_skb(skb);
}

static int raw_enable_filters(struct net *net, struct net_device *dev,
			      struct sock *sk, struct can_filter *filter,
			      int count)
{
	int err = 0;
	int i;

	for (i = 0; i < count; i++) {
		err = can_rx_register(net, dev, filter[i].can_id,
				      filter[i].can_mask,
				      raw_rcv, sk, "raw", sk);
		if (err) {
			 
			while (--i >= 0)
				can_rx_unregister(net, dev, filter[i].can_id,
						  filter[i].can_mask,
						  raw_rcv, sk);
			break;
		}
	}

	return err;
}

static int raw_enable_errfilter(struct net *net, struct net_device *dev,
				struct sock *sk, can_err_mask_t err_mask)
{
	int err = 0;

	if (err_mask)
		err = can_rx_register(net, dev, 0, err_mask | CAN_ERR_FLAG,
				      raw_rcv, sk, "raw", sk);

	return err;
}

static void raw_disable_filters(struct net *net, struct net_device *dev,
				struct sock *sk, struct can_filter *filter,
				int count)
{
	int i;

	for (i = 0; i < count; i++)
		can_rx_unregister(net, dev, filter[i].can_id,
				  filter[i].can_mask, raw_rcv, sk);
}

static inline void raw_disable_errfilter(struct net *net,
					 struct net_device *dev,
					 struct sock *sk,
					 can_err_mask_t err_mask)

{
	if (err_mask)
		can_rx_unregister(net, dev, 0, err_mask | CAN_ERR_FLAG,
				  raw_rcv, sk);
}

static inline void raw_disable_allfilters(struct net *net,
					  struct net_device *dev,
					  struct sock *sk)
{
	struct raw_sock *ro = raw_sk(sk);

	raw_disable_filters(net, dev, sk, ro->filter, ro->count);
	raw_disable_errfilter(net, dev, sk, ro->err_mask);
}

static int raw_enable_allfilters(struct net *net, struct net_device *dev,
				 struct sock *sk)
{
	struct raw_sock *ro = raw_sk(sk);
	int err;

	err = raw_enable_filters(net, dev, sk, ro->filter, ro->count);
	if (!err) {
		err = raw_enable_errfilter(net, dev, sk, ro->err_mask);
		if (err)
			raw_disable_filters(net, dev, sk, ro->filter,
					    ro->count);
	}

	return err;
}

static void raw_notify(struct raw_sock *ro, unsigned long msg,
		       struct net_device *dev)
{
	struct sock *sk = &ro->sk;

	if (!net_eq(dev_net(dev), sock_net(sk)))
		return;

	if (ro->dev != dev)
		return;

	switch (msg) {
	case NETDEV_UNREGISTER:
		lock_sock(sk);
		 
		if (ro->bound) {
			raw_disable_allfilters(dev_net(dev), dev, sk);
			netdev_put(dev, &ro->dev_tracker);
		}

		if (ro->count > 1)
			kfree(ro->filter);

		ro->ifindex = 0;
		ro->bound = 0;
		ro->dev = NULL;
		ro->count = 0;
		release_sock(sk);

		sk->sk_err = ENODEV;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);
		break;

	case NETDEV_DOWN:
		sk->sk_err = ENETDOWN;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);
		break;
	}
}

static int raw_notifier(struct notifier_block *nb, unsigned long msg,
			void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	if (dev->type != ARPHRD_CAN)
		return NOTIFY_DONE;
	if (msg != NETDEV_UNREGISTER && msg != NETDEV_DOWN)
		return NOTIFY_DONE;
	if (unlikely(raw_busy_notifier))  
		return NOTIFY_DONE;

	spin_lock(&raw_notifier_lock);
	list_for_each_entry(raw_busy_notifier, &raw_notifier_list, notifier) {
		spin_unlock(&raw_notifier_lock);
		raw_notify(raw_busy_notifier, msg, dev);
		spin_lock(&raw_notifier_lock);
	}
	raw_busy_notifier = NULL;
	spin_unlock(&raw_notifier_lock);
	return NOTIFY_DONE;
}

static int raw_init(struct sock *sk)
{
	struct raw_sock *ro = raw_sk(sk);

	ro->bound            = 0;
	ro->ifindex          = 0;
	ro->dev              = NULL;

	 
	ro->dfilter.can_id   = 0;
	ro->dfilter.can_mask = MASK_ALL;
	ro->filter           = &ro->dfilter;
	ro->count            = 1;

	 
	ro->loopback         = 1;
	ro->recv_own_msgs    = 0;
	ro->fd_frames        = 0;
	ro->xl_frames        = 0;
	ro->join_filters     = 0;

	 
	ro->uniq = alloc_percpu(struct uniqframe);
	if (unlikely(!ro->uniq))
		return -ENOMEM;

	 
	spin_lock(&raw_notifier_lock);
	list_add_tail(&ro->notifier, &raw_notifier_list);
	spin_unlock(&raw_notifier_lock);

	return 0;
}

static int raw_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct raw_sock *ro;

	if (!sk)
		return 0;

	ro = raw_sk(sk);

	spin_lock(&raw_notifier_lock);
	while (raw_busy_notifier == ro) {
		spin_unlock(&raw_notifier_lock);
		schedule_timeout_uninterruptible(1);
		spin_lock(&raw_notifier_lock);
	}
	list_del(&ro->notifier);
	spin_unlock(&raw_notifier_lock);

	rtnl_lock();
	lock_sock(sk);

	 
	if (ro->bound) {
		if (ro->dev) {
			raw_disable_allfilters(dev_net(ro->dev), ro->dev, sk);
			netdev_put(ro->dev, &ro->dev_tracker);
		} else {
			raw_disable_allfilters(sock_net(sk), NULL, sk);
		}
	}

	if (ro->count > 1)
		kfree(ro->filter);

	ro->ifindex = 0;
	ro->bound = 0;
	ro->dev = NULL;
	ro->count = 0;
	free_percpu(ro->uniq);

	sock_orphan(sk);
	sock->sk = NULL;

	release_sock(sk);
	rtnl_unlock();

	sock_put(sk);

	return 0;
}

static int raw_bind(struct socket *sock, struct sockaddr *uaddr, int len)
{
	struct sockaddr_can *addr = (struct sockaddr_can *)uaddr;
	struct sock *sk = sock->sk;
	struct raw_sock *ro = raw_sk(sk);
	struct net_device *dev = NULL;
	int ifindex;
	int err = 0;
	int notify_enetdown = 0;

	if (len < RAW_MIN_NAMELEN)
		return -EINVAL;
	if (addr->can_family != AF_CAN)
		return -EINVAL;

	rtnl_lock();
	lock_sock(sk);

	if (ro->bound && addr->can_ifindex == ro->ifindex)
		goto out;

	if (addr->can_ifindex) {
		dev = dev_get_by_index(sock_net(sk), addr->can_ifindex);
		if (!dev) {
			err = -ENODEV;
			goto out;
		}
		if (dev->type != ARPHRD_CAN) {
			err = -ENODEV;
			goto out_put_dev;
		}

		if (!(dev->flags & IFF_UP))
			notify_enetdown = 1;

		ifindex = dev->ifindex;

		 
		err = raw_enable_allfilters(sock_net(sk), dev, sk);
		if (err)
			goto out_put_dev;

	} else {
		ifindex = 0;

		 
		err = raw_enable_allfilters(sock_net(sk), NULL, sk);
	}

	if (!err) {
		if (ro->bound) {
			 
			if (ro->dev) {
				raw_disable_allfilters(dev_net(ro->dev),
						       ro->dev, sk);
				 
				netdev_put(ro->dev, &ro->dev_tracker);
			} else {
				raw_disable_allfilters(sock_net(sk), NULL, sk);
			}
		}
		ro->ifindex = ifindex;
		ro->bound = 1;
		 
		ro->dev = dev;
		if (ro->dev)
			netdev_hold(ro->dev, &ro->dev_tracker, GFP_KERNEL);
	}

out_put_dev:
	 
	if (dev)
		dev_put(dev);
out:
	release_sock(sk);
	rtnl_unlock();

	if (notify_enetdown) {
		sk->sk_err = ENETDOWN;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);
	}

	return err;
}

static int raw_getname(struct socket *sock, struct sockaddr *uaddr,
		       int peer)
{
	struct sockaddr_can *addr = (struct sockaddr_can *)uaddr;
	struct sock *sk = sock->sk;
	struct raw_sock *ro = raw_sk(sk);

	if (peer)
		return -EOPNOTSUPP;

	memset(addr, 0, RAW_MIN_NAMELEN);
	addr->can_family  = AF_CAN;
	addr->can_ifindex = ro->ifindex;

	return RAW_MIN_NAMELEN;
}

static int raw_setsockopt(struct socket *sock, int level, int optname,
			  sockptr_t optval, unsigned int optlen)
{
	struct sock *sk = sock->sk;
	struct raw_sock *ro = raw_sk(sk);
	struct can_filter *filter = NULL;   
	struct can_filter sfilter;          
	struct net_device *dev = NULL;
	can_err_mask_t err_mask = 0;
	int fd_frames;
	int count = 0;
	int err = 0;

	if (level != SOL_CAN_RAW)
		return -EINVAL;

	switch (optname) {
	case CAN_RAW_FILTER:
		if (optlen % sizeof(struct can_filter) != 0)
			return -EINVAL;

		if (optlen > CAN_RAW_FILTER_MAX * sizeof(struct can_filter))
			return -EINVAL;

		count = optlen / sizeof(struct can_filter);

		if (count > 1) {
			 
			filter = memdup_sockptr(optval, optlen);
			if (IS_ERR(filter))
				return PTR_ERR(filter);
		} else if (count == 1) {
			if (copy_from_sockptr(&sfilter, optval, sizeof(sfilter)))
				return -EFAULT;
		}

		rtnl_lock();
		lock_sock(sk);

		dev = ro->dev;
		if (ro->bound && dev) {
			if (dev->reg_state != NETREG_REGISTERED) {
				if (count > 1)
					kfree(filter);
				err = -ENODEV;
				goto out_fil;
			}
		}

		if (ro->bound) {
			 
			if (count == 1)
				err = raw_enable_filters(sock_net(sk), dev, sk,
							 &sfilter, 1);
			else
				err = raw_enable_filters(sock_net(sk), dev, sk,
							 filter, count);
			if (err) {
				if (count > 1)
					kfree(filter);
				goto out_fil;
			}

			 
			raw_disable_filters(sock_net(sk), dev, sk, ro->filter,
					    ro->count);
		}

		 
		if (ro->count > 1)
			kfree(ro->filter);

		 
		if (count == 1) {
			 
			ro->dfilter = sfilter;
			filter = &ro->dfilter;
		}
		ro->filter = filter;
		ro->count  = count;

 out_fil:
		release_sock(sk);
		rtnl_unlock();

		break;

	case CAN_RAW_ERR_FILTER:
		if (optlen != sizeof(err_mask))
			return -EINVAL;

		if (copy_from_sockptr(&err_mask, optval, optlen))
			return -EFAULT;

		err_mask &= CAN_ERR_MASK;

		rtnl_lock();
		lock_sock(sk);

		dev = ro->dev;
		if (ro->bound && dev) {
			if (dev->reg_state != NETREG_REGISTERED) {
				err = -ENODEV;
				goto out_err;
			}
		}

		 
		if (ro->bound) {
			 
			err = raw_enable_errfilter(sock_net(sk), dev, sk,
						   err_mask);

			if (err)
				goto out_err;

			 
			raw_disable_errfilter(sock_net(sk), dev, sk,
					      ro->err_mask);
		}

		 
		ro->err_mask = err_mask;

 out_err:
		release_sock(sk);
		rtnl_unlock();

		break;

	case CAN_RAW_LOOPBACK:
		if (optlen != sizeof(ro->loopback))
			return -EINVAL;

		if (copy_from_sockptr(&ro->loopback, optval, optlen))
			return -EFAULT;

		break;

	case CAN_RAW_RECV_OWN_MSGS:
		if (optlen != sizeof(ro->recv_own_msgs))
			return -EINVAL;

		if (copy_from_sockptr(&ro->recv_own_msgs, optval, optlen))
			return -EFAULT;

		break;

	case CAN_RAW_FD_FRAMES:
		if (optlen != sizeof(fd_frames))
			return -EINVAL;

		if (copy_from_sockptr(&fd_frames, optval, optlen))
			return -EFAULT;

		 
		if (ro->xl_frames && !fd_frames)
			return -EINVAL;

		ro->fd_frames = fd_frames;
		break;

	case CAN_RAW_XL_FRAMES:
		if (optlen != sizeof(ro->xl_frames))
			return -EINVAL;

		if (copy_from_sockptr(&ro->xl_frames, optval, optlen))
			return -EFAULT;

		 
		if (ro->xl_frames)
			ro->fd_frames = ro->xl_frames;
		break;

	case CAN_RAW_JOIN_FILTERS:
		if (optlen != sizeof(ro->join_filters))
			return -EINVAL;

		if (copy_from_sockptr(&ro->join_filters, optval, optlen))
			return -EFAULT;

		break;

	default:
		return -ENOPROTOOPT;
	}
	return err;
}

static int raw_getsockopt(struct socket *sock, int level, int optname,
			  char __user *optval, int __user *optlen)
{
	struct sock *sk = sock->sk;
	struct raw_sock *ro = raw_sk(sk);
	int len;
	void *val;
	int err = 0;

	if (level != SOL_CAN_RAW)
		return -EINVAL;
	if (get_user(len, optlen))
		return -EFAULT;
	if (len < 0)
		return -EINVAL;

	switch (optname) {
	case CAN_RAW_FILTER:
		lock_sock(sk);
		if (ro->count > 0) {
			int fsize = ro->count * sizeof(struct can_filter);

			 
			if (len < fsize) {
				 
				err = -ERANGE;
				if (put_user(fsize, optlen))
					err = -EFAULT;
			} else {
				if (len > fsize)
					len = fsize;
				if (copy_to_user(optval, ro->filter, len))
					err = -EFAULT;
			}
		} else {
			len = 0;
		}
		release_sock(sk);

		if (!err)
			err = put_user(len, optlen);
		return err;

	case CAN_RAW_ERR_FILTER:
		if (len > sizeof(can_err_mask_t))
			len = sizeof(can_err_mask_t);
		val = &ro->err_mask;
		break;

	case CAN_RAW_LOOPBACK:
		if (len > sizeof(int))
			len = sizeof(int);
		val = &ro->loopback;
		break;

	case CAN_RAW_RECV_OWN_MSGS:
		if (len > sizeof(int))
			len = sizeof(int);
		val = &ro->recv_own_msgs;
		break;

	case CAN_RAW_FD_FRAMES:
		if (len > sizeof(int))
			len = sizeof(int);
		val = &ro->fd_frames;
		break;

	case CAN_RAW_XL_FRAMES:
		if (len > sizeof(int))
			len = sizeof(int);
		val = &ro->xl_frames;
		break;

	case CAN_RAW_JOIN_FILTERS:
		if (len > sizeof(int))
			len = sizeof(int);
		val = &ro->join_filters;
		break;

	default:
		return -ENOPROTOOPT;
	}

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval, val, len))
		return -EFAULT;
	return 0;
}

static bool raw_bad_txframe(struct raw_sock *ro, struct sk_buff *skb, int mtu)
{
	 
	if (can_is_can_skb(skb))
		return false;

	 
	if (ro->fd_frames && can_is_canfd_skb(skb) &&
	    (mtu == CANFD_MTU || can_is_canxl_dev_mtu(mtu)))
		return false;

	 
	if (ro->xl_frames && can_is_canxl_skb(skb) &&
	    can_is_canxl_dev_mtu(mtu))
		return false;

	return true;
}

static int raw_sendmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock *sk = sock->sk;
	struct raw_sock *ro = raw_sk(sk);
	struct sockcm_cookie sockc;
	struct sk_buff *skb;
	struct net_device *dev;
	int ifindex;
	int err = -EINVAL;

	 
	if (size < CANXL_HDR_SIZE + CANXL_MIN_DLEN || size > CANXL_MTU)
		return -EINVAL;

	if (msg->msg_name) {
		DECLARE_SOCKADDR(struct sockaddr_can *, addr, msg->msg_name);

		if (msg->msg_namelen < RAW_MIN_NAMELEN)
			return -EINVAL;

		if (addr->can_family != AF_CAN)
			return -EINVAL;

		ifindex = addr->can_ifindex;
	} else {
		ifindex = ro->ifindex;
	}

	dev = dev_get_by_index(sock_net(sk), ifindex);
	if (!dev)
		return -ENXIO;

	skb = sock_alloc_send_skb(sk, size + sizeof(struct can_skb_priv),
				  msg->msg_flags & MSG_DONTWAIT, &err);
	if (!skb)
		goto put_dev;

	can_skb_reserve(skb);
	can_skb_prv(skb)->ifindex = dev->ifindex;
	can_skb_prv(skb)->skbcnt = 0;

	 
	err = memcpy_from_msg(skb_put(skb, size), msg, size);
	if (err < 0)
		goto free_skb;

	err = -EINVAL;
	if (raw_bad_txframe(ro, skb, dev->mtu))
		goto free_skb;

	sockcm_init(&sockc, sk);
	if (msg->msg_controllen) {
		err = sock_cmsg_send(sk, msg, &sockc);
		if (unlikely(err))
			goto free_skb;
	}

	skb->dev = dev;
	skb->priority = sk->sk_priority;
	skb->mark = READ_ONCE(sk->sk_mark);
	skb->tstamp = sockc.transmit_time;

	skb_setup_tx_timestamp(skb, sockc.tsflags);

	err = can_send(skb, ro->loopback);

	dev_put(dev);

	if (err)
		goto send_failed;

	return size;

free_skb:
	kfree_skb(skb);
put_dev:
	dev_put(dev);
send_failed:
	return err;
}

static int raw_recvmsg(struct socket *sock, struct msghdr *msg, size_t size,
		       int flags)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	int err = 0;

	if (flags & MSG_ERRQUEUE)
		return sock_recv_errqueue(sk, msg, size,
					  SOL_CAN_RAW, SCM_CAN_RAW_ERRQUEUE);

	skb = skb_recv_datagram(sk, flags, &err);
	if (!skb)
		return err;

	if (size < skb->len)
		msg->msg_flags |= MSG_TRUNC;
	else
		size = skb->len;

	err = memcpy_to_msg(msg, skb->data, size);
	if (err < 0) {
		skb_free_datagram(sk, skb);
		return err;
	}

	sock_recv_cmsgs(msg, sk, skb);

	if (msg->msg_name) {
		__sockaddr_check_size(RAW_MIN_NAMELEN);
		msg->msg_namelen = RAW_MIN_NAMELEN;
		memcpy(msg->msg_name, skb->cb, msg->msg_namelen);
	}

	 
	msg->msg_flags |= *(raw_flags(skb));

	skb_free_datagram(sk, skb);

	return size;
}

static int raw_sock_no_ioctlcmd(struct socket *sock, unsigned int cmd,
				unsigned long arg)
{
	 
	return -ENOIOCTLCMD;
}

static const struct proto_ops raw_ops = {
	.family        = PF_CAN,
	.release       = raw_release,
	.bind          = raw_bind,
	.connect       = sock_no_connect,
	.socketpair    = sock_no_socketpair,
	.accept        = sock_no_accept,
	.getname       = raw_getname,
	.poll          = datagram_poll,
	.ioctl         = raw_sock_no_ioctlcmd,
	.gettstamp     = sock_gettstamp,
	.listen        = sock_no_listen,
	.shutdown      = sock_no_shutdown,
	.setsockopt    = raw_setsockopt,
	.getsockopt    = raw_getsockopt,
	.sendmsg       = raw_sendmsg,
	.recvmsg       = raw_recvmsg,
	.mmap          = sock_no_mmap,
};

static struct proto raw_proto __read_mostly = {
	.name       = "CAN_RAW",
	.owner      = THIS_MODULE,
	.obj_size   = sizeof(struct raw_sock),
	.init       = raw_init,
};

static const struct can_proto raw_can_proto = {
	.type       = SOCK_RAW,
	.protocol   = CAN_RAW,
	.ops        = &raw_ops,
	.prot       = &raw_proto,
};

static struct notifier_block canraw_notifier = {
	.notifier_call = raw_notifier
};

static __init int raw_module_init(void)
{
	int err;

	pr_info("can: raw protocol\n");

	err = register_netdevice_notifier(&canraw_notifier);
	if (err)
		return err;

	err = can_proto_register(&raw_can_proto);
	if (err < 0) {
		pr_err("can: registration of raw protocol failed\n");
		goto register_proto_failed;
	}

	return 0;

register_proto_failed:
	unregister_netdevice_notifier(&canraw_notifier);
	return err;
}

static __exit void raw_module_exit(void)
{
	can_proto_unregister(&raw_can_proto);
	unregister_netdevice_notifier(&canraw_notifier);
}

module_init(raw_module_init);
module_exit(raw_module_exit);
