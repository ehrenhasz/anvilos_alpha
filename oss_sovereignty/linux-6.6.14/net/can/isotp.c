
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/wait.h>
#include <linux/uio.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/can.h>
#include <linux/can/core.h>
#include <linux/can/skb.h>
#include <linux/can/isotp.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/net_namespace.h>

MODULE_DESCRIPTION("PF_CAN isotp 15765-2:2016 protocol");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Oliver Hartkopp <socketcan@hartkopp.net>");
MODULE_ALIAS("can-proto-6");

#define ISOTP_MIN_NAMELEN CAN_REQUIRED_SIZE(struct sockaddr_can, can_addr.tp)

#define SINGLE_MASK(id) (((id) & CAN_EFF_FLAG) ? \
			 (CAN_EFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG) : \
			 (CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG))

 
#define DEFAULT_MAX_PDU_SIZE 8300

 
#define MAX_12BIT_PDU_SIZE 4095

 
#define MAX_PDU_SIZE (1025 * 1024U)

static unsigned int max_pdu_size __read_mostly = DEFAULT_MAX_PDU_SIZE;
module_param(max_pdu_size, uint, 0444);
MODULE_PARM_DESC(max_pdu_size, "maximum isotp pdu size (default "
		 __stringify(DEFAULT_MAX_PDU_SIZE) ")");

 
#define N_PCI_SF 0x00	 
#define N_PCI_FF 0x10	 
#define N_PCI_CF 0x20	 
#define N_PCI_FC 0x30	 

#define N_PCI_SZ 1	 
#define SF_PCI_SZ4 1	 
#define SF_PCI_SZ8 2	 
#define FF_PCI_SZ12 2	 
#define FF_PCI_SZ32 6	 
#define FC_CONTENT_SZ 3	 

#define ISOTP_CHECK_PADDING (CAN_ISOTP_CHK_PAD_LEN | CAN_ISOTP_CHK_PAD_DATA)
#define ISOTP_ALL_BC_FLAGS (CAN_ISOTP_SF_BROADCAST | CAN_ISOTP_CF_BROADCAST)

 
#define ISOTP_FC_CTS 0		 
#define ISOTP_FC_WT 1		 
#define ISOTP_FC_OVFLW 2	 

#define ISOTP_FC_TIMEOUT 1	 
#define ISOTP_ECHO_TIMEOUT 2	 

enum {
	ISOTP_IDLE = 0,
	ISOTP_WAIT_FIRST_FC,
	ISOTP_WAIT_FC,
	ISOTP_WAIT_DATA,
	ISOTP_SENDING,
	ISOTP_SHUTDOWN,
};

struct tpcon {
	u8 *buf;
	unsigned int buflen;
	unsigned int len;
	unsigned int idx;
	u32 state;
	u8 bs;
	u8 sn;
	u8 ll_dl;
	u8 sbuf[DEFAULT_MAX_PDU_SIZE];
};

struct isotp_sock {
	struct sock sk;
	int bound;
	int ifindex;
	canid_t txid;
	canid_t rxid;
	ktime_t tx_gap;
	ktime_t lastrxcf_tstamp;
	struct hrtimer rxtimer, txtimer, txfrtimer;
	struct can_isotp_options opt;
	struct can_isotp_fc_options rxfc, txfc;
	struct can_isotp_ll_options ll;
	u32 frame_txtime;
	u32 force_tx_stmin;
	u32 force_rx_stmin;
	u32 cfecho;  
	struct tpcon rx, tx;
	struct list_head notifier;
	wait_queue_head_t wait;
	spinlock_t rx_lock;  
};

static LIST_HEAD(isotp_notifier_list);
static DEFINE_SPINLOCK(isotp_notifier_lock);
static struct isotp_sock *isotp_busy_notifier;

static inline struct isotp_sock *isotp_sk(const struct sock *sk)
{
	return (struct isotp_sock *)sk;
}

static u32 isotp_bc_flags(struct isotp_sock *so)
{
	return so->opt.flags & ISOTP_ALL_BC_FLAGS;
}

static bool isotp_register_rxid(struct isotp_sock *so)
{
	 
	return (isotp_bc_flags(so) == 0);
}

static enum hrtimer_restart isotp_rx_timer_handler(struct hrtimer *hrtimer)
{
	struct isotp_sock *so = container_of(hrtimer, struct isotp_sock,
					     rxtimer);
	struct sock *sk = &so->sk;

	if (so->rx.state == ISOTP_WAIT_DATA) {
		 

		 
		sk->sk_err = ETIMEDOUT;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);

		 
		so->rx.state = ISOTP_IDLE;
	}

	return HRTIMER_NORESTART;
}

static int isotp_send_fc(struct sock *sk, int ae, u8 flowstatus)
{
	struct net_device *dev;
	struct sk_buff *nskb;
	struct canfd_frame *ncf;
	struct isotp_sock *so = isotp_sk(sk);
	int can_send_ret;

	nskb = alloc_skb(so->ll.mtu + sizeof(struct can_skb_priv), gfp_any());
	if (!nskb)
		return 1;

	dev = dev_get_by_index(sock_net(sk), so->ifindex);
	if (!dev) {
		kfree_skb(nskb);
		return 1;
	}

	can_skb_reserve(nskb);
	can_skb_prv(nskb)->ifindex = dev->ifindex;
	can_skb_prv(nskb)->skbcnt = 0;

	nskb->dev = dev;
	can_skb_set_owner(nskb, sk);
	ncf = (struct canfd_frame *)nskb->data;
	skb_put_zero(nskb, so->ll.mtu);

	 
	ncf->can_id = so->txid;

	if (so->opt.flags & CAN_ISOTP_TX_PADDING) {
		memset(ncf->data, so->opt.txpad_content, CAN_MAX_DLEN);
		ncf->len = CAN_MAX_DLEN;
	} else {
		ncf->len = ae + FC_CONTENT_SZ;
	}

	ncf->data[ae] = N_PCI_FC | flowstatus;
	ncf->data[ae + 1] = so->rxfc.bs;
	ncf->data[ae + 2] = so->rxfc.stmin;

	if (ae)
		ncf->data[0] = so->opt.ext_address;

	ncf->flags = so->ll.tx_flags;

	can_send_ret = can_send(nskb, 1);
	if (can_send_ret)
		pr_notice_once("can-isotp: %s: can_send_ret %pe\n",
			       __func__, ERR_PTR(can_send_ret));

	dev_put(dev);

	 
	so->rx.bs = 0;

	 
	so->lastrxcf_tstamp = ktime_set(0, 0);

	 
	hrtimer_start(&so->rxtimer, ktime_set(ISOTP_FC_TIMEOUT, 0),
		      HRTIMER_MODE_REL_SOFT);
	return 0;
}

static void isotp_rcv_skb(struct sk_buff *skb, struct sock *sk)
{
	struct sockaddr_can *addr = (struct sockaddr_can *)skb->cb;

	BUILD_BUG_ON(sizeof(skb->cb) < sizeof(struct sockaddr_can));

	memset(addr, 0, sizeof(*addr));
	addr->can_family = AF_CAN;
	addr->can_ifindex = skb->dev->ifindex;

	if (sock_queue_rcv_skb(sk, skb) < 0)
		kfree_skb(skb);
}

static u8 padlen(u8 datalen)
{
	static const u8 plen[] = {
		8, 8, 8, 8, 8, 8, 8, 8, 8,	 
		12, 12, 12, 12,			 
		16, 16, 16, 16,			 
		20, 20, 20, 20,			 
		24, 24, 24, 24,			 
		32, 32, 32, 32, 32, 32, 32, 32,	 
		48, 48, 48, 48, 48, 48, 48, 48,	 
		48, 48, 48, 48, 48, 48, 48, 48	 
	};

	if (datalen > 48)
		return 64;

	return plen[datalen];
}

 
static int check_optimized(struct canfd_frame *cf, int start_index)
{
	 
	if (cf->len <= CAN_MAX_DLEN)
		return (cf->len != start_index);

	 
	return (cf->len != padlen(start_index));
}

 
static int check_pad(struct isotp_sock *so, struct canfd_frame *cf,
		     int start_index, u8 content)
{
	int i;

	 
	if (!(so->opt.flags & CAN_ISOTP_RX_PADDING)) {
		if (so->opt.flags & CAN_ISOTP_CHK_PAD_LEN)
			return check_optimized(cf, start_index);

		 
		return 1;
	}

	 
	if ((so->opt.flags & CAN_ISOTP_CHK_PAD_LEN) &&
	    cf->len != padlen(cf->len))
		return 1;

	 
	if (so->opt.flags & CAN_ISOTP_CHK_PAD_DATA) {
		for (i = start_index; i < cf->len; i++)
			if (cf->data[i] != content)
				return 1;
	}
	return 0;
}

static void isotp_send_cframe(struct isotp_sock *so);

static int isotp_rcv_fc(struct isotp_sock *so, struct canfd_frame *cf, int ae)
{
	struct sock *sk = &so->sk;

	if (so->tx.state != ISOTP_WAIT_FC &&
	    so->tx.state != ISOTP_WAIT_FIRST_FC)
		return 0;

	hrtimer_cancel(&so->txtimer);

	if ((cf->len < ae + FC_CONTENT_SZ) ||
	    ((so->opt.flags & ISOTP_CHECK_PADDING) &&
	     check_pad(so, cf, ae + FC_CONTENT_SZ, so->opt.rxpad_content))) {
		 
		sk->sk_err = EBADMSG;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);

		so->tx.state = ISOTP_IDLE;
		wake_up_interruptible(&so->wait);
		return 1;
	}

	 
	if (so->tx.state == ISOTP_WAIT_FIRST_FC) {
		so->txfc.bs = cf->data[ae + 1];
		so->txfc.stmin = cf->data[ae + 2];

		 
		if (so->txfc.stmin > 0x7F &&
		    (so->txfc.stmin < 0xF1 || so->txfc.stmin > 0xF9))
			so->txfc.stmin = 0x7F;

		so->tx_gap = ktime_set(0, 0);
		 
		so->tx_gap = ktime_add_ns(so->tx_gap, so->frame_txtime);
		 
		if (so->opt.flags & CAN_ISOTP_FORCE_TXSTMIN)
			so->tx_gap = ktime_add_ns(so->tx_gap,
						  so->force_tx_stmin);
		else if (so->txfc.stmin < 0x80)
			so->tx_gap = ktime_add_ns(so->tx_gap,
						  so->txfc.stmin * 1000000);
		else
			so->tx_gap = ktime_add_ns(so->tx_gap,
						  (so->txfc.stmin - 0xF0)
						  * 100000);
		so->tx.state = ISOTP_WAIT_FC;
	}

	switch (cf->data[ae] & 0x0F) {
	case ISOTP_FC_CTS:
		so->tx.bs = 0;
		so->tx.state = ISOTP_SENDING;
		 
		hrtimer_start(&so->txtimer, ktime_set(ISOTP_ECHO_TIMEOUT, 0),
			      HRTIMER_MODE_REL_SOFT);
		isotp_send_cframe(so);
		break;

	case ISOTP_FC_WT:
		 
		hrtimer_start(&so->txtimer, ktime_set(ISOTP_FC_TIMEOUT, 0),
			      HRTIMER_MODE_REL_SOFT);
		break;

	case ISOTP_FC_OVFLW:
		 
		sk->sk_err = EMSGSIZE;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);
		fallthrough;

	default:
		 
		so->tx.state = ISOTP_IDLE;
		wake_up_interruptible(&so->wait);
	}
	return 0;
}

static int isotp_rcv_sf(struct sock *sk, struct canfd_frame *cf, int pcilen,
			struct sk_buff *skb, int len)
{
	struct isotp_sock *so = isotp_sk(sk);
	struct sk_buff *nskb;

	hrtimer_cancel(&so->rxtimer);
	so->rx.state = ISOTP_IDLE;

	if (!len || len > cf->len - pcilen)
		return 1;

	if ((so->opt.flags & ISOTP_CHECK_PADDING) &&
	    check_pad(so, cf, pcilen + len, so->opt.rxpad_content)) {
		 
		sk->sk_err = EBADMSG;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);
		return 1;
	}

	nskb = alloc_skb(len, gfp_any());
	if (!nskb)
		return 1;

	memcpy(skb_put(nskb, len), &cf->data[pcilen], len);

	nskb->tstamp = skb->tstamp;
	nskb->dev = skb->dev;
	isotp_rcv_skb(nskb, sk);
	return 0;
}

static int isotp_rcv_ff(struct sock *sk, struct canfd_frame *cf, int ae)
{
	struct isotp_sock *so = isotp_sk(sk);
	int i;
	int off;
	int ff_pci_sz;

	hrtimer_cancel(&so->rxtimer);
	so->rx.state = ISOTP_IDLE;

	 
	so->rx.ll_dl = padlen(cf->len);

	 
	if (cf->len != so->rx.ll_dl)
		return 1;

	 
	so->rx.len = (cf->data[ae] & 0x0F) << 8;
	so->rx.len += cf->data[ae + 1];

	 
	if (so->rx.len) {
		ff_pci_sz = FF_PCI_SZ12;
	} else {
		 
		so->rx.len = cf->data[ae + 2] << 24;
		so->rx.len += cf->data[ae + 3] << 16;
		so->rx.len += cf->data[ae + 4] << 8;
		so->rx.len += cf->data[ae + 5];
		ff_pci_sz = FF_PCI_SZ32;
	}

	 
	off = (so->rx.ll_dl > CAN_MAX_DLEN) ? 1 : 0;

	if (so->rx.len + ae + off + ff_pci_sz < so->rx.ll_dl)
		return 1;

	 
	if (so->rx.len > so->rx.buflen && so->rx.buflen < max_pdu_size) {
		u8 *newbuf = kmalloc(max_pdu_size, GFP_ATOMIC);

		if (newbuf) {
			so->rx.buf = newbuf;
			so->rx.buflen = max_pdu_size;
		}
	}

	if (so->rx.len > so->rx.buflen) {
		 
		isotp_send_fc(sk, ae, ISOTP_FC_OVFLW);
		return 1;
	}

	 
	so->rx.idx = 0;
	for (i = ae + ff_pci_sz; i < so->rx.ll_dl; i++)
		so->rx.buf[so->rx.idx++] = cf->data[i];

	 
	so->rx.sn = 1;
	so->rx.state = ISOTP_WAIT_DATA;

	 
	if (so->opt.flags & CAN_ISOTP_LISTEN_MODE)
		return 0;

	 
	isotp_send_fc(sk, ae, ISOTP_FC_CTS);
	return 0;
}

static int isotp_rcv_cf(struct sock *sk, struct canfd_frame *cf, int ae,
			struct sk_buff *skb)
{
	struct isotp_sock *so = isotp_sk(sk);
	struct sk_buff *nskb;
	int i;

	if (so->rx.state != ISOTP_WAIT_DATA)
		return 0;

	 
	if (so->opt.flags & CAN_ISOTP_FORCE_RXSTMIN) {
		if (ktime_to_ns(ktime_sub(skb->tstamp, so->lastrxcf_tstamp)) <
		    so->force_rx_stmin)
			return 0;

		so->lastrxcf_tstamp = skb->tstamp;
	}

	hrtimer_cancel(&so->rxtimer);

	 
	if (cf->len > so->rx.ll_dl)
		return 1;

	 
	if (cf->len < so->rx.ll_dl) {
		 
		if (so->rx.len - so->rx.idx > so->rx.ll_dl - ae - N_PCI_SZ)
			return 1;
	}

	if ((cf->data[ae] & 0x0F) != so->rx.sn) {
		 
		sk->sk_err = EILSEQ;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);

		 
		so->rx.state = ISOTP_IDLE;
		return 1;
	}
	so->rx.sn++;
	so->rx.sn %= 16;

	for (i = ae + N_PCI_SZ; i < cf->len; i++) {
		so->rx.buf[so->rx.idx++] = cf->data[i];
		if (so->rx.idx >= so->rx.len)
			break;
	}

	if (so->rx.idx >= so->rx.len) {
		 
		so->rx.state = ISOTP_IDLE;

		if ((so->opt.flags & ISOTP_CHECK_PADDING) &&
		    check_pad(so, cf, i + 1, so->opt.rxpad_content)) {
			 
			sk->sk_err = EBADMSG;
			if (!sock_flag(sk, SOCK_DEAD))
				sk_error_report(sk);
			return 1;
		}

		nskb = alloc_skb(so->rx.len, gfp_any());
		if (!nskb)
			return 1;

		memcpy(skb_put(nskb, so->rx.len), so->rx.buf,
		       so->rx.len);

		nskb->tstamp = skb->tstamp;
		nskb->dev = skb->dev;
		isotp_rcv_skb(nskb, sk);
		return 0;
	}

	 
	if (!so->rxfc.bs || ++so->rx.bs < so->rxfc.bs) {
		 
		hrtimer_start(&so->rxtimer, ktime_set(ISOTP_FC_TIMEOUT, 0),
			      HRTIMER_MODE_REL_SOFT);
		return 0;
	}

	 
	if (so->opt.flags & CAN_ISOTP_LISTEN_MODE)
		return 0;

	 
	isotp_send_fc(sk, ae, ISOTP_FC_CTS);
	return 0;
}

static void isotp_rcv(struct sk_buff *skb, void *data)
{
	struct sock *sk = (struct sock *)data;
	struct isotp_sock *so = isotp_sk(sk);
	struct canfd_frame *cf;
	int ae = (so->opt.flags & CAN_ISOTP_EXTEND_ADDR) ? 1 : 0;
	u8 n_pci_type, sf_dl;

	 
	if (skb->len != so->ll.mtu)
		return;

	cf = (struct canfd_frame *)skb->data;

	 
	if (ae && cf->data[0] != so->opt.rx_ext_address)
		return;

	n_pci_type = cf->data[ae] & 0xF0;

	 
	spin_lock(&so->rx_lock);

	if (so->opt.flags & CAN_ISOTP_HALF_DUPLEX) {
		 
		if ((so->tx.state != ISOTP_IDLE && n_pci_type != N_PCI_FC) ||
		    (so->rx.state != ISOTP_IDLE && n_pci_type == N_PCI_FC))
			goto out_unlock;
	}

	switch (n_pci_type) {
	case N_PCI_FC:
		 
		isotp_rcv_fc(so, cf, ae);
		break;

	case N_PCI_SF:
		 

		 
		sf_dl = cf->data[ae] & 0x0F;

		if (cf->len <= CAN_MAX_DLEN) {
			isotp_rcv_sf(sk, cf, SF_PCI_SZ4 + ae, skb, sf_dl);
		} else {
			if (can_is_canfd_skb(skb)) {
				 
				if (sf_dl == 0)
					isotp_rcv_sf(sk, cf, SF_PCI_SZ8 + ae, skb,
						     cf->data[SF_PCI_SZ4 + ae]);
			}
		}
		break;

	case N_PCI_FF:
		 
		isotp_rcv_ff(sk, cf, ae);
		break;

	case N_PCI_CF:
		 
		isotp_rcv_cf(sk, cf, ae, skb);
		break;
	}

out_unlock:
	spin_unlock(&so->rx_lock);
}

static void isotp_fill_dataframe(struct canfd_frame *cf, struct isotp_sock *so,
				 int ae, int off)
{
	int pcilen = N_PCI_SZ + ae + off;
	int space = so->tx.ll_dl - pcilen;
	int num = min_t(int, so->tx.len - so->tx.idx, space);
	int i;

	cf->can_id = so->txid;
	cf->len = num + pcilen;

	if (num < space) {
		if (so->opt.flags & CAN_ISOTP_TX_PADDING) {
			 
			cf->len = padlen(cf->len);
			memset(cf->data, so->opt.txpad_content, cf->len);
		} else if (cf->len > CAN_MAX_DLEN) {
			 
			cf->len = padlen(cf->len);
			memset(cf->data, CAN_ISOTP_DEFAULT_PAD_CONTENT,
			       cf->len);
		}
	}

	for (i = 0; i < num; i++)
		cf->data[pcilen + i] = so->tx.buf[so->tx.idx++];

	if (ae)
		cf->data[0] = so->opt.ext_address;
}

static void isotp_send_cframe(struct isotp_sock *so)
{
	struct sock *sk = &so->sk;
	struct sk_buff *skb;
	struct net_device *dev;
	struct canfd_frame *cf;
	int can_send_ret;
	int ae = (so->opt.flags & CAN_ISOTP_EXTEND_ADDR) ? 1 : 0;

	dev = dev_get_by_index(sock_net(sk), so->ifindex);
	if (!dev)
		return;

	skb = alloc_skb(so->ll.mtu + sizeof(struct can_skb_priv), GFP_ATOMIC);
	if (!skb) {
		dev_put(dev);
		return;
	}

	can_skb_reserve(skb);
	can_skb_prv(skb)->ifindex = dev->ifindex;
	can_skb_prv(skb)->skbcnt = 0;

	cf = (struct canfd_frame *)skb->data;
	skb_put_zero(skb, so->ll.mtu);

	 
	isotp_fill_dataframe(cf, so, ae, 0);

	 
	cf->data[ae] = N_PCI_CF | so->tx.sn++;
	so->tx.sn %= 16;
	so->tx.bs++;

	cf->flags = so->ll.tx_flags;

	skb->dev = dev;
	can_skb_set_owner(skb, sk);

	 
	if (so->cfecho)
		pr_notice_once("can-isotp: cfecho is %08X != 0\n", so->cfecho);

	 
	so->cfecho = *(u32 *)cf->data;

	 
	can_send_ret = can_send(skb, 1);
	if (can_send_ret) {
		pr_notice_once("can-isotp: %s: can_send_ret %pe\n",
			       __func__, ERR_PTR(can_send_ret));
		if (can_send_ret == -ENOBUFS)
			pr_notice_once("can-isotp: tx queue is full\n");
	}
	dev_put(dev);
}

static void isotp_create_fframe(struct canfd_frame *cf, struct isotp_sock *so,
				int ae)
{
	int i;
	int ff_pci_sz;

	cf->can_id = so->txid;
	cf->len = so->tx.ll_dl;
	if (ae)
		cf->data[0] = so->opt.ext_address;

	 
	if (so->tx.len > MAX_12BIT_PDU_SIZE) {
		 
		cf->data[ae] = N_PCI_FF;
		cf->data[ae + 1] = 0;
		cf->data[ae + 2] = (u8)(so->tx.len >> 24) & 0xFFU;
		cf->data[ae + 3] = (u8)(so->tx.len >> 16) & 0xFFU;
		cf->data[ae + 4] = (u8)(so->tx.len >> 8) & 0xFFU;
		cf->data[ae + 5] = (u8)so->tx.len & 0xFFU;
		ff_pci_sz = FF_PCI_SZ32;
	} else {
		 
		cf->data[ae] = (u8)(so->tx.len >> 8) | N_PCI_FF;
		cf->data[ae + 1] = (u8)so->tx.len & 0xFFU;
		ff_pci_sz = FF_PCI_SZ12;
	}

	 
	for (i = ae + ff_pci_sz; i < so->tx.ll_dl; i++)
		cf->data[i] = so->tx.buf[so->tx.idx++];

	so->tx.sn = 1;
}

static void isotp_rcv_echo(struct sk_buff *skb, void *data)
{
	struct sock *sk = (struct sock *)data;
	struct isotp_sock *so = isotp_sk(sk);
	struct canfd_frame *cf = (struct canfd_frame *)skb->data;

	 
	if (skb->sk != sk || so->cfecho != *(u32 *)cf->data)
		return;

	 
	hrtimer_cancel(&so->txtimer);

	 
	so->cfecho = 0;

	if (so->tx.idx >= so->tx.len) {
		 
		so->tx.state = ISOTP_IDLE;
		wake_up_interruptible(&so->wait);
		return;
	}

	if (so->txfc.bs && so->tx.bs >= so->txfc.bs) {
		 
		so->tx.state = ISOTP_WAIT_FC;
		hrtimer_start(&so->txtimer, ktime_set(ISOTP_FC_TIMEOUT, 0),
			      HRTIMER_MODE_REL_SOFT);
		return;
	}

	 
	if (!so->tx_gap) {
		 
		hrtimer_start(&so->txtimer, ktime_set(ISOTP_ECHO_TIMEOUT, 0),
			      HRTIMER_MODE_REL_SOFT);
		isotp_send_cframe(so);
		return;
	}

	 
	hrtimer_start(&so->txfrtimer, so->tx_gap, HRTIMER_MODE_REL_SOFT);
}

static enum hrtimer_restart isotp_tx_timer_handler(struct hrtimer *hrtimer)
{
	struct isotp_sock *so = container_of(hrtimer, struct isotp_sock,
					     txtimer);
	struct sock *sk = &so->sk;

	 
	if (so->tx.state == ISOTP_IDLE || so->tx.state == ISOTP_SHUTDOWN)
		return HRTIMER_NORESTART;

	 

	 
	sk->sk_err = ECOMM;
	if (!sock_flag(sk, SOCK_DEAD))
		sk_error_report(sk);

	 
	so->tx.state = ISOTP_IDLE;
	wake_up_interruptible(&so->wait);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart isotp_txfr_timer_handler(struct hrtimer *hrtimer)
{
	struct isotp_sock *so = container_of(hrtimer, struct isotp_sock,
					     txfrtimer);

	 
	hrtimer_start(&so->txtimer, ktime_set(ISOTP_ECHO_TIMEOUT, 0),
		      HRTIMER_MODE_REL_SOFT);

	 
	if (so->tx.state == ISOTP_SENDING && !so->cfecho)
		isotp_send_cframe(so);

	return HRTIMER_NORESTART;
}

static int isotp_sendmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock *sk = sock->sk;
	struct isotp_sock *so = isotp_sk(sk);
	struct sk_buff *skb;
	struct net_device *dev;
	struct canfd_frame *cf;
	int ae = (so->opt.flags & CAN_ISOTP_EXTEND_ADDR) ? 1 : 0;
	int wait_tx_done = (so->opt.flags & CAN_ISOTP_WAIT_TX_DONE) ? 1 : 0;
	s64 hrtimer_sec = ISOTP_ECHO_TIMEOUT;
	int off;
	int err;

	if (!so->bound || so->tx.state == ISOTP_SHUTDOWN)
		return -EADDRNOTAVAIL;

	while (cmpxchg(&so->tx.state, ISOTP_IDLE, ISOTP_SENDING) != ISOTP_IDLE) {
		 
		if (msg->msg_flags & MSG_DONTWAIT)
			return -EAGAIN;

		if (so->tx.state == ISOTP_SHUTDOWN)
			return -EADDRNOTAVAIL;

		 
		err = wait_event_interruptible(so->wait, so->tx.state == ISOTP_IDLE);
		if (err)
			goto err_event_drop;
	}

	 
	if (size > so->tx.buflen && so->tx.buflen < max_pdu_size) {
		u8 *newbuf = kmalloc(max_pdu_size, GFP_KERNEL);

		if (newbuf) {
			so->tx.buf = newbuf;
			so->tx.buflen = max_pdu_size;
		}
	}

	if (!size || size > so->tx.buflen) {
		err = -EINVAL;
		goto err_out_drop;
	}

	 
	off = (so->tx.ll_dl > CAN_MAX_DLEN) ? 1 : 0;

	 
	if ((isotp_bc_flags(so) == CAN_ISOTP_SF_BROADCAST) &&
	    (size > so->tx.ll_dl - SF_PCI_SZ4 - ae - off)) {
		err = -EINVAL;
		goto err_out_drop;
	}

	err = memcpy_from_msg(so->tx.buf, msg, size);
	if (err < 0)
		goto err_out_drop;

	dev = dev_get_by_index(sock_net(sk), so->ifindex);
	if (!dev) {
		err = -ENXIO;
		goto err_out_drop;
	}

	skb = sock_alloc_send_skb(sk, so->ll.mtu + sizeof(struct can_skb_priv),
				  msg->msg_flags & MSG_DONTWAIT, &err);
	if (!skb) {
		dev_put(dev);
		goto err_out_drop;
	}

	can_skb_reserve(skb);
	can_skb_prv(skb)->ifindex = dev->ifindex;
	can_skb_prv(skb)->skbcnt = 0;

	so->tx.len = size;
	so->tx.idx = 0;

	cf = (struct canfd_frame *)skb->data;
	skb_put_zero(skb, so->ll.mtu);

	 
	if (so->cfecho)
		pr_notice_once("can-isotp: uninit cfecho %08X\n", so->cfecho);

	 
	if (size <= so->tx.ll_dl - SF_PCI_SZ4 - ae - off) {
		 
		if (size <= CAN_MAX_DLEN - SF_PCI_SZ4 - ae)
			off = 0;

		isotp_fill_dataframe(cf, so, ae, off);

		 
		cf->data[ae] = N_PCI_SF;

		 
		if (off)
			cf->data[SF_PCI_SZ4 + ae] = size;
		else
			cf->data[ae] |= size;

		 
		so->cfecho = *(u32 *)cf->data;
	} else {
		 

		isotp_create_fframe(cf, so, ae);

		if (isotp_bc_flags(so) == CAN_ISOTP_CF_BROADCAST) {
			 
			if (so->opt.flags & CAN_ISOTP_FORCE_TXSTMIN)
				so->tx_gap = ktime_set(0, so->force_tx_stmin);
			else
				so->tx_gap = ktime_set(0, so->frame_txtime);

			 
			so->txfc.bs = 0;

			 
			so->cfecho = *(u32 *)cf->data;
		} else {
			 
			so->tx.state = ISOTP_WAIT_FIRST_FC;

			 
			hrtimer_sec = ISOTP_FC_TIMEOUT;

			 
			so->cfecho = 0;
		}
	}

	hrtimer_start(&so->txtimer, ktime_set(hrtimer_sec, 0),
		      HRTIMER_MODE_REL_SOFT);

	 
	cf->flags = so->ll.tx_flags;

	skb->dev = dev;
	skb->sk = sk;
	err = can_send(skb, 1);
	dev_put(dev);
	if (err) {
		pr_notice_once("can-isotp: %s: can_send_ret %pe\n",
			       __func__, ERR_PTR(err));

		 
		hrtimer_cancel(&so->txtimer);

		 
		so->cfecho = 0;

		goto err_out_drop;
	}

	if (wait_tx_done) {
		 
		err = wait_event_interruptible(so->wait, so->tx.state == ISOTP_IDLE);
		if (err)
			goto err_event_drop;

		err = sock_error(sk);
		if (err)
			return err;
	}

	return size;

err_event_drop:
	 
	so->tx.state = ISOTP_IDLE;
	hrtimer_cancel(&so->txfrtimer);
	hrtimer_cancel(&so->txtimer);
err_out_drop:
	 
	so->tx.state = ISOTP_IDLE;
	wake_up_interruptible(&so->wait);

	return err;
}

static int isotp_recvmsg(struct socket *sock, struct msghdr *msg, size_t size,
			 int flags)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	struct isotp_sock *so = isotp_sk(sk);
	int ret = 0;

	if (flags & ~(MSG_DONTWAIT | MSG_TRUNC | MSG_PEEK | MSG_CMSG_COMPAT))
		return -EINVAL;

	if (!so->bound)
		return -EADDRNOTAVAIL;

	skb = skb_recv_datagram(sk, flags, &ret);
	if (!skb)
		return ret;

	if (size < skb->len)
		msg->msg_flags |= MSG_TRUNC;
	else
		size = skb->len;

	ret = memcpy_to_msg(msg, skb->data, size);
	if (ret < 0)
		goto out_err;

	sock_recv_cmsgs(msg, sk, skb);

	if (msg->msg_name) {
		__sockaddr_check_size(ISOTP_MIN_NAMELEN);
		msg->msg_namelen = ISOTP_MIN_NAMELEN;
		memcpy(msg->msg_name, skb->cb, msg->msg_namelen);
	}

	 
	ret = (flags & MSG_TRUNC) ? skb->len : size;

out_err:
	skb_free_datagram(sk, skb);

	return ret;
}

static int isotp_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct isotp_sock *so;
	struct net *net;

	if (!sk)
		return 0;

	so = isotp_sk(sk);
	net = sock_net(sk);

	 
	while (wait_event_interruptible(so->wait, so->tx.state == ISOTP_IDLE) == 0 &&
	       cmpxchg(&so->tx.state, ISOTP_IDLE, ISOTP_SHUTDOWN) != ISOTP_IDLE)
		;

	 
	so->tx.state = ISOTP_SHUTDOWN;
	so->rx.state = ISOTP_IDLE;

	spin_lock(&isotp_notifier_lock);
	while (isotp_busy_notifier == so) {
		spin_unlock(&isotp_notifier_lock);
		schedule_timeout_uninterruptible(1);
		spin_lock(&isotp_notifier_lock);
	}
	list_del(&so->notifier);
	spin_unlock(&isotp_notifier_lock);

	lock_sock(sk);

	 
	if (so->bound) {
		if (so->ifindex) {
			struct net_device *dev;

			dev = dev_get_by_index(net, so->ifindex);
			if (dev) {
				if (isotp_register_rxid(so))
					can_rx_unregister(net, dev, so->rxid,
							  SINGLE_MASK(so->rxid),
							  isotp_rcv, sk);

				can_rx_unregister(net, dev, so->txid,
						  SINGLE_MASK(so->txid),
						  isotp_rcv_echo, sk);
				dev_put(dev);
				synchronize_rcu();
			}
		}
	}

	hrtimer_cancel(&so->txfrtimer);
	hrtimer_cancel(&so->txtimer);
	hrtimer_cancel(&so->rxtimer);

	so->ifindex = 0;
	so->bound = 0;

	if (so->rx.buf != so->rx.sbuf)
		kfree(so->rx.buf);

	if (so->tx.buf != so->tx.sbuf)
		kfree(so->tx.buf);

	sock_orphan(sk);
	sock->sk = NULL;

	release_sock(sk);
	sock_put(sk);

	return 0;
}

static int isotp_bind(struct socket *sock, struct sockaddr *uaddr, int len)
{
	struct sockaddr_can *addr = (struct sockaddr_can *)uaddr;
	struct sock *sk = sock->sk;
	struct isotp_sock *so = isotp_sk(sk);
	struct net *net = sock_net(sk);
	int ifindex;
	struct net_device *dev;
	canid_t tx_id = addr->can_addr.tp.tx_id;
	canid_t rx_id = addr->can_addr.tp.rx_id;
	int err = 0;
	int notify_enetdown = 0;

	if (len < ISOTP_MIN_NAMELEN)
		return -EINVAL;

	if (addr->can_family != AF_CAN)
		return -EINVAL;

	 
	if (tx_id & CAN_EFF_FLAG)
		tx_id &= (CAN_EFF_FLAG | CAN_EFF_MASK);
	else
		tx_id &= CAN_SFF_MASK;

	 
	if (tx_id != addr->can_addr.tp.tx_id)
		return -EINVAL;

	 
	if (isotp_register_rxid(so)) {
		if (rx_id & CAN_EFF_FLAG)
			rx_id &= (CAN_EFF_FLAG | CAN_EFF_MASK);
		else
			rx_id &= CAN_SFF_MASK;

		 
		if (rx_id != addr->can_addr.tp.rx_id)
			return -EINVAL;
	}

	if (!addr->can_ifindex)
		return -ENODEV;

	lock_sock(sk);

	if (so->bound) {
		err = -EINVAL;
		goto out;
	}

	 
	if (isotp_register_rxid(so) && rx_id == tx_id) {
		err = -EADDRNOTAVAIL;
		goto out;
	}

	dev = dev_get_by_index(net, addr->can_ifindex);
	if (!dev) {
		err = -ENODEV;
		goto out;
	}
	if (dev->type != ARPHRD_CAN) {
		dev_put(dev);
		err = -ENODEV;
		goto out;
	}
	if (dev->mtu < so->ll.mtu) {
		dev_put(dev);
		err = -EINVAL;
		goto out;
	}
	if (!(dev->flags & IFF_UP))
		notify_enetdown = 1;

	ifindex = dev->ifindex;

	if (isotp_register_rxid(so))
		can_rx_register(net, dev, rx_id, SINGLE_MASK(rx_id),
				isotp_rcv, sk, "isotp", sk);

	 
	so->cfecho = 0;

	 
	can_rx_register(net, dev, tx_id, SINGLE_MASK(tx_id),
			isotp_rcv_echo, sk, "isotpe", sk);

	dev_put(dev);

	 
	so->ifindex = ifindex;
	so->rxid = rx_id;
	so->txid = tx_id;
	so->bound = 1;

out:
	release_sock(sk);

	if (notify_enetdown) {
		sk->sk_err = ENETDOWN;
		if (!sock_flag(sk, SOCK_DEAD))
			sk_error_report(sk);
	}

	return err;
}

static int isotp_getname(struct socket *sock, struct sockaddr *uaddr, int peer)
{
	struct sockaddr_can *addr = (struct sockaddr_can *)uaddr;
	struct sock *sk = sock->sk;
	struct isotp_sock *so = isotp_sk(sk);

	if (peer)
		return -EOPNOTSUPP;

	memset(addr, 0, ISOTP_MIN_NAMELEN);
	addr->can_family = AF_CAN;
	addr->can_ifindex = so->ifindex;
	addr->can_addr.tp.rx_id = so->rxid;
	addr->can_addr.tp.tx_id = so->txid;

	return ISOTP_MIN_NAMELEN;
}

static int isotp_setsockopt_locked(struct socket *sock, int level, int optname,
			    sockptr_t optval, unsigned int optlen)
{
	struct sock *sk = sock->sk;
	struct isotp_sock *so = isotp_sk(sk);
	int ret = 0;

	if (so->bound)
		return -EISCONN;

	switch (optname) {
	case CAN_ISOTP_OPTS:
		if (optlen != sizeof(struct can_isotp_options))
			return -EINVAL;

		if (copy_from_sockptr(&so->opt, optval, optlen))
			return -EFAULT;

		 
		if (!(so->opt.flags & CAN_ISOTP_RX_EXT_ADDR))
			so->opt.rx_ext_address = so->opt.ext_address;

		 
		if (isotp_bc_flags(so) == ISOTP_ALL_BC_FLAGS) {
			 
			so->opt.flags &= ~CAN_ISOTP_CF_BROADCAST;

			 
			ret = -EINVAL;
		}

		 
		if (so->opt.frame_txtime) {
			if (so->opt.frame_txtime == CAN_ISOTP_FRAME_TXTIME_ZERO)
				so->frame_txtime = 0;
			else
				so->frame_txtime = so->opt.frame_txtime;
		}
		break;

	case CAN_ISOTP_RECV_FC:
		if (optlen != sizeof(struct can_isotp_fc_options))
			return -EINVAL;

		if (copy_from_sockptr(&so->rxfc, optval, optlen))
			return -EFAULT;
		break;

	case CAN_ISOTP_TX_STMIN:
		if (optlen != sizeof(u32))
			return -EINVAL;

		if (copy_from_sockptr(&so->force_tx_stmin, optval, optlen))
			return -EFAULT;
		break;

	case CAN_ISOTP_RX_STMIN:
		if (optlen != sizeof(u32))
			return -EINVAL;

		if (copy_from_sockptr(&so->force_rx_stmin, optval, optlen))
			return -EFAULT;
		break;

	case CAN_ISOTP_LL_OPTS:
		if (optlen == sizeof(struct can_isotp_ll_options)) {
			struct can_isotp_ll_options ll;

			if (copy_from_sockptr(&ll, optval, optlen))
				return -EFAULT;

			 
			if (ll.tx_dl != padlen(ll.tx_dl))
				return -EINVAL;

			if (ll.mtu != CAN_MTU && ll.mtu != CANFD_MTU)
				return -EINVAL;

			if (ll.mtu == CAN_MTU &&
			    (ll.tx_dl > CAN_MAX_DLEN || ll.tx_flags != 0))
				return -EINVAL;

			memcpy(&so->ll, &ll, sizeof(ll));

			 
			so->tx.ll_dl = ll.tx_dl;
		} else {
			return -EINVAL;
		}
		break;

	default:
		ret = -ENOPROTOOPT;
	}

	return ret;
}

static int isotp_setsockopt(struct socket *sock, int level, int optname,
			    sockptr_t optval, unsigned int optlen)

{
	struct sock *sk = sock->sk;
	int ret;

	if (level != SOL_CAN_ISOTP)
		return -EINVAL;

	lock_sock(sk);
	ret = isotp_setsockopt_locked(sock, level, optname, optval, optlen);
	release_sock(sk);
	return ret;
}

static int isotp_getsockopt(struct socket *sock, int level, int optname,
			    char __user *optval, int __user *optlen)
{
	struct sock *sk = sock->sk;
	struct isotp_sock *so = isotp_sk(sk);
	int len;
	void *val;

	if (level != SOL_CAN_ISOTP)
		return -EINVAL;
	if (get_user(len, optlen))
		return -EFAULT;
	if (len < 0)
		return -EINVAL;

	switch (optname) {
	case CAN_ISOTP_OPTS:
		len = min_t(int, len, sizeof(struct can_isotp_options));
		val = &so->opt;
		break;

	case CAN_ISOTP_RECV_FC:
		len = min_t(int, len, sizeof(struct can_isotp_fc_options));
		val = &so->rxfc;
		break;

	case CAN_ISOTP_TX_STMIN:
		len = min_t(int, len, sizeof(u32));
		val = &so->force_tx_stmin;
		break;

	case CAN_ISOTP_RX_STMIN:
		len = min_t(int, len, sizeof(u32));
		val = &so->force_rx_stmin;
		break;

	case CAN_ISOTP_LL_OPTS:
		len = min_t(int, len, sizeof(struct can_isotp_ll_options));
		val = &so->ll;
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

static void isotp_notify(struct isotp_sock *so, unsigned long msg,
			 struct net_device *dev)
{
	struct sock *sk = &so->sk;

	if (!net_eq(dev_net(dev), sock_net(sk)))
		return;

	if (so->ifindex != dev->ifindex)
		return;

	switch (msg) {
	case NETDEV_UNREGISTER:
		lock_sock(sk);
		 
		if (so->bound) {
			if (isotp_register_rxid(so))
				can_rx_unregister(dev_net(dev), dev, so->rxid,
						  SINGLE_MASK(so->rxid),
						  isotp_rcv, sk);

			can_rx_unregister(dev_net(dev), dev, so->txid,
					  SINGLE_MASK(so->txid),
					  isotp_rcv_echo, sk);
		}

		so->ifindex = 0;
		so->bound  = 0;
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

static int isotp_notifier(struct notifier_block *nb, unsigned long msg,
			  void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	if (dev->type != ARPHRD_CAN)
		return NOTIFY_DONE;
	if (msg != NETDEV_UNREGISTER && msg != NETDEV_DOWN)
		return NOTIFY_DONE;
	if (unlikely(isotp_busy_notifier))  
		return NOTIFY_DONE;

	spin_lock(&isotp_notifier_lock);
	list_for_each_entry(isotp_busy_notifier, &isotp_notifier_list, notifier) {
		spin_unlock(&isotp_notifier_lock);
		isotp_notify(isotp_busy_notifier, msg, dev);
		spin_lock(&isotp_notifier_lock);
	}
	isotp_busy_notifier = NULL;
	spin_unlock(&isotp_notifier_lock);
	return NOTIFY_DONE;
}

static int isotp_init(struct sock *sk)
{
	struct isotp_sock *so = isotp_sk(sk);

	so->ifindex = 0;
	so->bound = 0;

	so->opt.flags = CAN_ISOTP_DEFAULT_FLAGS;
	so->opt.ext_address = CAN_ISOTP_DEFAULT_EXT_ADDRESS;
	so->opt.rx_ext_address = CAN_ISOTP_DEFAULT_EXT_ADDRESS;
	so->opt.rxpad_content = CAN_ISOTP_DEFAULT_PAD_CONTENT;
	so->opt.txpad_content = CAN_ISOTP_DEFAULT_PAD_CONTENT;
	so->opt.frame_txtime = CAN_ISOTP_DEFAULT_FRAME_TXTIME;
	so->frame_txtime = CAN_ISOTP_DEFAULT_FRAME_TXTIME;
	so->rxfc.bs = CAN_ISOTP_DEFAULT_RECV_BS;
	so->rxfc.stmin = CAN_ISOTP_DEFAULT_RECV_STMIN;
	so->rxfc.wftmax = CAN_ISOTP_DEFAULT_RECV_WFTMAX;
	so->ll.mtu = CAN_ISOTP_DEFAULT_LL_MTU;
	so->ll.tx_dl = CAN_ISOTP_DEFAULT_LL_TX_DL;
	so->ll.tx_flags = CAN_ISOTP_DEFAULT_LL_TX_FLAGS;

	 
	so->tx.ll_dl = so->ll.tx_dl;

	so->rx.state = ISOTP_IDLE;
	so->tx.state = ISOTP_IDLE;

	so->rx.buf = so->rx.sbuf;
	so->tx.buf = so->tx.sbuf;
	so->rx.buflen = ARRAY_SIZE(so->rx.sbuf);
	so->tx.buflen = ARRAY_SIZE(so->tx.sbuf);

	hrtimer_init(&so->rxtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
	so->rxtimer.function = isotp_rx_timer_handler;
	hrtimer_init(&so->txtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
	so->txtimer.function = isotp_tx_timer_handler;
	hrtimer_init(&so->txfrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
	so->txfrtimer.function = isotp_txfr_timer_handler;

	init_waitqueue_head(&so->wait);
	spin_lock_init(&so->rx_lock);

	spin_lock(&isotp_notifier_lock);
	list_add_tail(&so->notifier, &isotp_notifier_list);
	spin_unlock(&isotp_notifier_lock);

	return 0;
}

static __poll_t isotp_poll(struct file *file, struct socket *sock, poll_table *wait)
{
	struct sock *sk = sock->sk;
	struct isotp_sock *so = isotp_sk(sk);

	__poll_t mask = datagram_poll(file, sock, wait);
	poll_wait(file, &so->wait, wait);

	 
	if ((mask & EPOLLWRNORM) && (so->tx.state != ISOTP_IDLE))
		mask &= ~(EPOLLOUT | EPOLLWRNORM);

	return mask;
}

static int isotp_sock_no_ioctlcmd(struct socket *sock, unsigned int cmd,
				  unsigned long arg)
{
	 
	return -ENOIOCTLCMD;
}

static const struct proto_ops isotp_ops = {
	.family = PF_CAN,
	.release = isotp_release,
	.bind = isotp_bind,
	.connect = sock_no_connect,
	.socketpair = sock_no_socketpair,
	.accept = sock_no_accept,
	.getname = isotp_getname,
	.poll = isotp_poll,
	.ioctl = isotp_sock_no_ioctlcmd,
	.gettstamp = sock_gettstamp,
	.listen = sock_no_listen,
	.shutdown = sock_no_shutdown,
	.setsockopt = isotp_setsockopt,
	.getsockopt = isotp_getsockopt,
	.sendmsg = isotp_sendmsg,
	.recvmsg = isotp_recvmsg,
	.mmap = sock_no_mmap,
};

static struct proto isotp_proto __read_mostly = {
	.name = "CAN_ISOTP",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct isotp_sock),
	.init = isotp_init,
};

static const struct can_proto isotp_can_proto = {
	.type = SOCK_DGRAM,
	.protocol = CAN_ISOTP,
	.ops = &isotp_ops,
	.prot = &isotp_proto,
};

static struct notifier_block canisotp_notifier = {
	.notifier_call = isotp_notifier
};

static __init int isotp_module_init(void)
{
	int err;

	max_pdu_size = max_t(unsigned int, max_pdu_size, MAX_12BIT_PDU_SIZE);
	max_pdu_size = min_t(unsigned int, max_pdu_size, MAX_PDU_SIZE);

	pr_info("can: isotp protocol (max_pdu_size %d)\n", max_pdu_size);

	err = can_proto_register(&isotp_can_proto);
	if (err < 0)
		pr_err("can: registration of isotp protocol failed %pe\n", ERR_PTR(err));
	else
		register_netdevice_notifier(&canisotp_notifier);

	return err;
}

static __exit void isotp_module_exit(void)
{
	can_proto_unregister(&isotp_can_proto);
	unregister_netdevice_notifier(&canisotp_notifier);
}

module_init(isotp_module_init);
module_exit(isotp_module_exit);
