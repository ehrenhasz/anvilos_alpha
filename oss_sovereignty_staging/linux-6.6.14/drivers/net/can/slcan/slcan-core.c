 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>

#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/skb.h>

#include "slcan.h"

MODULE_ALIAS_LDISC(N_SLCAN);
MODULE_DESCRIPTION("serial line CAN interface");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oliver Hartkopp <socketcan@hartkopp.net>");
MODULE_AUTHOR("Dario Binacchi <dario.binacchi@amarulasolutions.com>");

 
#define SLCAN_MTU (sizeof("T1111222281122334455667788EA5F\r") + 1)

#define SLCAN_CMD_LEN 1
#define SLCAN_SFF_ID_LEN 3
#define SLCAN_EFF_ID_LEN 8
#define SLCAN_STATE_LEN 1
#define SLCAN_STATE_BE_RXCNT_LEN 3
#define SLCAN_STATE_BE_TXCNT_LEN 3
#define SLCAN_STATE_FRAME_LEN       (1 + SLCAN_CMD_LEN + \
				     SLCAN_STATE_BE_RXCNT_LEN + \
				     SLCAN_STATE_BE_TXCNT_LEN)
struct slcan {
	struct can_priv         can;

	 
	struct tty_struct	*tty;		 
	struct net_device	*dev;		 
	spinlock_t		lock;
	struct work_struct	tx_work;	 

	 
	unsigned char		rbuff[SLCAN_MTU];	 
	int			rcount;          
	unsigned char		xbuff[SLCAN_MTU];	 
	unsigned char		*xhead;          
	int			xleft;           

	unsigned long		flags;		 
#define SLF_ERROR		0                
#define SLF_XCMD		1                
	unsigned long           cmd_flags;       
#define CF_ERR_RST		0                
	wait_queue_head_t       xcmd_wait;       
						 
};

static const u32 slcan_bitrate_const[] = {
	10000, 20000, 50000, 100000, 125000,
	250000, 500000, 800000, 1000000
};

bool slcan_err_rst_on_open(struct net_device *ndev)
{
	struct slcan *sl = netdev_priv(ndev);

	return !!test_bit(CF_ERR_RST, &sl->cmd_flags);
}

int slcan_enable_err_rst_on_open(struct net_device *ndev, bool on)
{
	struct slcan *sl = netdev_priv(ndev);

	if (netif_running(ndev))
		return -EBUSY;

	if (on)
		set_bit(CF_ERR_RST, &sl->cmd_flags);
	else
		clear_bit(CF_ERR_RST, &sl->cmd_flags);

	return 0;
}

 

 

 

 
static void slcan_bump_frame(struct slcan *sl)
{
	struct sk_buff *skb;
	struct can_frame *cf;
	int i, tmp;
	u32 tmpid;
	char *cmd = sl->rbuff;

	skb = alloc_can_skb(sl->dev, &cf);
	if (unlikely(!skb)) {
		sl->dev->stats.rx_dropped++;
		return;
	}

	switch (*cmd) {
	case 'r':
		cf->can_id = CAN_RTR_FLAG;
		fallthrough;
	case 't':
		 
		cf->len = sl->rbuff[SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN];
		sl->rbuff[SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN] = 0;
		 
		cmd += SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN + 1;
		break;
	case 'R':
		cf->can_id = CAN_RTR_FLAG;
		fallthrough;
	case 'T':
		cf->can_id |= CAN_EFF_FLAG;
		 
		cf->len = sl->rbuff[SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN];
		sl->rbuff[SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN] = 0;
		 
		cmd += SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN + 1;
		break;
	default:
		goto decode_failed;
	}

	if (kstrtou32(sl->rbuff + SLCAN_CMD_LEN, 16, &tmpid))
		goto decode_failed;

	cf->can_id |= tmpid;

	 
	if (cf->len >= '0' && cf->len < '9')
		cf->len -= '0';
	else
		goto decode_failed;

	 
	if (!(cf->can_id & CAN_RTR_FLAG)) {
		for (i = 0; i < cf->len; i++) {
			tmp = hex_to_bin(*cmd++);
			if (tmp < 0)
				goto decode_failed;

			cf->data[i] = (tmp << 4);
			tmp = hex_to_bin(*cmd++);
			if (tmp < 0)
				goto decode_failed;

			cf->data[i] |= tmp;
		}
	}

	sl->dev->stats.rx_packets++;
	if (!(cf->can_id & CAN_RTR_FLAG))
		sl->dev->stats.rx_bytes += cf->len;

	netif_rx(skb);
	return;

decode_failed:
	sl->dev->stats.rx_errors++;
	dev_kfree_skb(skb);
}

 
static void slcan_bump_state(struct slcan *sl)
{
	struct net_device *dev = sl->dev;
	struct sk_buff *skb;
	struct can_frame *cf;
	char *cmd = sl->rbuff;
	u32 rxerr, txerr;
	enum can_state state, rx_state, tx_state;

	switch (cmd[1]) {
	case 'a':
		state = CAN_STATE_ERROR_ACTIVE;
		break;
	case 'w':
		state = CAN_STATE_ERROR_WARNING;
		break;
	case 'p':
		state = CAN_STATE_ERROR_PASSIVE;
		break;
	case 'b':
		state = CAN_STATE_BUS_OFF;
		break;
	default:
		return;
	}

	if (state == sl->can.state || sl->rcount < SLCAN_STATE_FRAME_LEN)
		return;

	cmd += SLCAN_STATE_BE_RXCNT_LEN + SLCAN_CMD_LEN + 1;
	cmd[SLCAN_STATE_BE_TXCNT_LEN] = 0;
	if (kstrtou32(cmd, 10, &txerr))
		return;

	*cmd = 0;
	cmd -= SLCAN_STATE_BE_RXCNT_LEN;
	if (kstrtou32(cmd, 10, &rxerr))
		return;

	skb = alloc_can_err_skb(dev, &cf);

	tx_state = txerr >= rxerr ? state : 0;
	rx_state = txerr <= rxerr ? state : 0;
	can_change_state(dev, cf, tx_state, rx_state);

	if (state == CAN_STATE_BUS_OFF) {
		can_bus_off(dev);
	} else if (skb) {
		cf->can_id |= CAN_ERR_CNT;
		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}

	if (skb)
		netif_rx(skb);
}

 
static void slcan_bump_err(struct slcan *sl)
{
	struct net_device *dev = sl->dev;
	struct sk_buff *skb;
	struct can_frame *cf;
	char *cmd = sl->rbuff;
	bool rx_errors = false, tx_errors = false, rx_over_errors = false;
	int i, len;

	 
	len = cmd[1];
	if (len >= '0' && len < '9')
		len -= '0';
	else
		return;

	if ((len + SLCAN_CMD_LEN + 1) > sl->rcount)
		return;

	skb = alloc_can_err_skb(dev, &cf);

	if (skb)
		cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;

	cmd += SLCAN_CMD_LEN + 1;
	for (i = 0; i < len; i++, cmd++) {
		switch (*cmd) {
		case 'a':
			netdev_dbg(dev, "ACK error\n");
			tx_errors = true;
			if (skb) {
				cf->can_id |= CAN_ERR_ACK;
				cf->data[3] = CAN_ERR_PROT_LOC_ACK;
			}

			break;
		case 'b':
			netdev_dbg(dev, "Bit0 error\n");
			tx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_BIT0;

			break;
		case 'B':
			netdev_dbg(dev, "Bit1 error\n");
			tx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_BIT1;

			break;
		case 'c':
			netdev_dbg(dev, "CRC error\n");
			rx_errors = true;
			if (skb) {
				cf->data[2] |= CAN_ERR_PROT_BIT;
				cf->data[3] = CAN_ERR_PROT_LOC_CRC_SEQ;
			}

			break;
		case 'f':
			netdev_dbg(dev, "Form Error\n");
			rx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_FORM;

			break;
		case 'o':
			netdev_dbg(dev, "Rx overrun error\n");
			rx_over_errors = true;
			rx_errors = true;
			if (skb) {
				cf->can_id |= CAN_ERR_CRTL;
				cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
			}

			break;
		case 'O':
			netdev_dbg(dev, "Tx overrun error\n");
			tx_errors = true;
			if (skb) {
				cf->can_id |= CAN_ERR_CRTL;
				cf->data[1] = CAN_ERR_CRTL_TX_OVERFLOW;
			}

			break;
		case 's':
			netdev_dbg(dev, "Stuff error\n");
			rx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_STUFF;

			break;
		default:
			if (skb)
				dev_kfree_skb(skb);

			return;
		}
	}

	if (rx_errors)
		dev->stats.rx_errors++;

	if (rx_over_errors)
		dev->stats.rx_over_errors++;

	if (tx_errors)
		dev->stats.tx_errors++;

	if (skb)
		netif_rx(skb);
}

static void slcan_bump(struct slcan *sl)
{
	switch (sl->rbuff[0]) {
	case 'r':
		fallthrough;
	case 't':
		fallthrough;
	case 'R':
		fallthrough;
	case 'T':
		return slcan_bump_frame(sl);
	case 'e':
		return slcan_bump_err(sl);
	case 's':
		return slcan_bump_state(sl);
	default:
		return;
	}
}

 
static void slcan_unesc(struct slcan *sl, unsigned char s)
{
	if ((s == '\r') || (s == '\a')) {  
		if (!test_and_clear_bit(SLF_ERROR, &sl->flags) &&
		    sl->rcount > 4)
			slcan_bump(sl);

		sl->rcount = 0;
	} else {
		if (!test_bit(SLF_ERROR, &sl->flags))  {
			if (sl->rcount < SLCAN_MTU)  {
				sl->rbuff[sl->rcount++] = s;
				return;
			}

			sl->dev->stats.rx_over_errors++;
			set_bit(SLF_ERROR, &sl->flags);
		}
	}
}

 

 
static void slcan_encaps(struct slcan *sl, struct can_frame *cf)
{
	int actual, i;
	unsigned char *pos;
	unsigned char *endpos;
	canid_t id = cf->can_id;

	pos = sl->xbuff;

	if (cf->can_id & CAN_RTR_FLAG)
		*pos = 'R';  
	else
		*pos = 'T';  

	 
	if (cf->can_id & CAN_EFF_FLAG) {
		id &= CAN_EFF_MASK;
		endpos = pos + SLCAN_EFF_ID_LEN;
	} else {
		*pos |= 0x20;  
		id &= CAN_SFF_MASK;
		endpos = pos + SLCAN_SFF_ID_LEN;
	}

	 
	pos++;
	while (endpos >= pos) {
		*endpos-- = hex_asc_upper[id & 0xf];
		id >>= 4;
	}

	pos += (cf->can_id & CAN_EFF_FLAG) ?
		SLCAN_EFF_ID_LEN : SLCAN_SFF_ID_LEN;

	*pos++ = cf->len + '0';

	 
	if (!(cf->can_id & CAN_RTR_FLAG)) {
		for (i = 0; i < cf->len; i++)
			pos = hex_byte_pack_upper(pos, cf->data[i]);

		sl->dev->stats.tx_bytes += cf->len;
	}

	*pos++ = '\r';

	 
	set_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	actual = sl->tty->ops->write(sl->tty, sl->xbuff, pos - sl->xbuff);
	sl->xleft = (pos - sl->xbuff) - actual;
	sl->xhead = sl->xbuff + actual;
}

 
static void slcan_transmit(struct work_struct *work)
{
	struct slcan *sl = container_of(work, struct slcan, tx_work);
	int actual;

	spin_lock_bh(&sl->lock);
	 
	if (unlikely(!netif_running(sl->dev)) &&
	    likely(!test_bit(SLF_XCMD, &sl->flags))) {
		spin_unlock_bh(&sl->lock);
		return;
	}

	if (sl->xleft <= 0)  {
		if (unlikely(test_bit(SLF_XCMD, &sl->flags))) {
			clear_bit(SLF_XCMD, &sl->flags);
			clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
			spin_unlock_bh(&sl->lock);
			wake_up(&sl->xcmd_wait);
			return;
		}

		 
		sl->dev->stats.tx_packets++;
		clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
		spin_unlock_bh(&sl->lock);
		netif_wake_queue(sl->dev);
		return;
	}

	actual = sl->tty->ops->write(sl->tty, sl->xhead, sl->xleft);
	sl->xleft -= actual;
	sl->xhead += actual;
	spin_unlock_bh(&sl->lock);
}

 
static void slcan_write_wakeup(struct tty_struct *tty)
{
	struct slcan *sl = tty->disc_data;

	schedule_work(&sl->tx_work);
}

 
static netdev_tx_t slcan_netdev_xmit(struct sk_buff *skb,
				     struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);

	if (can_dev_dropped_skb(dev, skb))
		return NETDEV_TX_OK;

	spin_lock(&sl->lock);
	if (!netif_running(dev))  {
		spin_unlock(&sl->lock);
		netdev_warn(dev, "xmit: iface is down\n");
		goto out;
	}
	if (!sl->tty) {
		spin_unlock(&sl->lock);
		goto out;
	}

	netif_stop_queue(sl->dev);
	slcan_encaps(sl, (struct can_frame *)skb->data);  
	spin_unlock(&sl->lock);

	skb_tx_timestamp(skb);

out:
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

 

static int slcan_transmit_cmd(struct slcan *sl, const unsigned char *cmd)
{
	int ret, actual, n;

	spin_lock(&sl->lock);
	if (!sl->tty) {
		spin_unlock(&sl->lock);
		return -ENODEV;
	}

	n = scnprintf(sl->xbuff, sizeof(sl->xbuff), "%s", cmd);
	set_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	actual = sl->tty->ops->write(sl->tty, sl->xbuff, n);
	sl->xleft = n - actual;
	sl->xhead = sl->xbuff + actual;
	set_bit(SLF_XCMD, &sl->flags);
	spin_unlock(&sl->lock);
	ret = wait_event_interruptible_timeout(sl->xcmd_wait,
					       !test_bit(SLF_XCMD, &sl->flags),
					       HZ);
	clear_bit(SLF_XCMD, &sl->flags);
	if (ret == -ERESTARTSYS)
		return ret;

	if (ret == 0)
		return -ETIMEDOUT;

	return 0;
}

 
static int slcan_netdev_close(struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);
	int err;

	if (sl->can.bittiming.bitrate &&
	    sl->can.bittiming.bitrate != CAN_BITRATE_UNKNOWN) {
		err = slcan_transmit_cmd(sl, "C\r");
		if (err)
			netdev_warn(dev,
				    "failed to send close command 'C\\r'\n");
	}

	 
	clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	flush_work(&sl->tx_work);

	netif_stop_queue(dev);
	sl->rcount   = 0;
	sl->xleft    = 0;
	close_candev(dev);
	sl->can.state = CAN_STATE_STOPPED;
	if (sl->can.bittiming.bitrate == CAN_BITRATE_UNKNOWN)
		sl->can.bittiming.bitrate = CAN_BITRATE_UNSET;

	return 0;
}

 
static int slcan_netdev_open(struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);
	unsigned char cmd[SLCAN_MTU];
	int err, s;

	 
	if (sl->can.bittiming.bitrate == CAN_BITRATE_UNSET)
		sl->can.bittiming.bitrate = CAN_BITRATE_UNKNOWN;

	err = open_candev(dev);
	if (err) {
		netdev_err(dev, "failed to open can device\n");
		return err;
	}

	if (sl->can.bittiming.bitrate != CAN_BITRATE_UNKNOWN) {
		for (s = 0; s < ARRAY_SIZE(slcan_bitrate_const); s++) {
			if (sl->can.bittiming.bitrate == slcan_bitrate_const[s])
				break;
		}

		 
		snprintf(cmd, sizeof(cmd), "C\rS%d\r", s);
		err = slcan_transmit_cmd(sl, cmd);
		if (err) {
			netdev_err(dev,
				   "failed to send bitrate command 'C\\rS%d\\r'\n",
				   s);
			goto cmd_transmit_failed;
		}

		if (test_bit(CF_ERR_RST, &sl->cmd_flags)) {
			err = slcan_transmit_cmd(sl, "F\r");
			if (err) {
				netdev_err(dev,
					   "failed to send error command 'F\\r'\n");
				goto cmd_transmit_failed;
			}
		}

		if (sl->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) {
			err = slcan_transmit_cmd(sl, "L\r");
			if (err) {
				netdev_err(dev,
					   "failed to send listen-only command 'L\\r'\n");
				goto cmd_transmit_failed;
			}
		} else {
			err = slcan_transmit_cmd(sl, "O\r");
			if (err) {
				netdev_err(dev,
					   "failed to send open command 'O\\r'\n");
				goto cmd_transmit_failed;
			}
		}
	}

	sl->can.state = CAN_STATE_ERROR_ACTIVE;
	netif_start_queue(dev);
	return 0;

cmd_transmit_failed:
	close_candev(dev);
	return err;
}

static const struct net_device_ops slcan_netdev_ops = {
	.ndo_open               = slcan_netdev_open,
	.ndo_stop               = slcan_netdev_close,
	.ndo_start_xmit         = slcan_netdev_xmit,
	.ndo_change_mtu         = can_change_mtu,
};

 

 
static void slcan_receive_buf(struct tty_struct *tty, const u8 *cp,
			      const u8 *fp, size_t count)
{
	struct slcan *sl = tty->disc_data;

	if (!netif_running(sl->dev))
		return;

	 
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))
				sl->dev->stats.rx_errors++;
			cp++;
			continue;
		}
		slcan_unesc(sl, *cp++);
	}
}

 
static int slcan_open(struct tty_struct *tty)
{
	struct net_device *dev;
	struct slcan *sl;
	int err;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!tty->ops->write)
		return -EOPNOTSUPP;

	dev = alloc_candev(sizeof(*sl), 1);
	if (!dev)
		return -ENFILE;

	sl = netdev_priv(dev);

	 
	tty->receive_room = 65536;  
	sl->rcount = 0;
	sl->xleft = 0;
	spin_lock_init(&sl->lock);
	INIT_WORK(&sl->tx_work, slcan_transmit);
	init_waitqueue_head(&sl->xcmd_wait);

	 
	sl->can.bitrate_const = slcan_bitrate_const;
	sl->can.bitrate_const_cnt = ARRAY_SIZE(slcan_bitrate_const);
	sl->can.ctrlmode_supported = CAN_CTRLMODE_LISTENONLY;

	 
	sl->dev	= dev;
	dev->netdev_ops = &slcan_netdev_ops;
	dev->ethtool_ops = &slcan_ethtool_ops;

	 
	sl->tty = tty;
	tty->disc_data = sl;

	err = register_candev(dev);
	if (err) {
		free_candev(dev);
		pr_err("can't register candev\n");
		return err;
	}

	netdev_info(dev, "slcan on %s.\n", tty->name);
	 
	return 0;
}

 
static void slcan_close(struct tty_struct *tty)
{
	struct slcan *sl = tty->disc_data;

	unregister_candev(sl->dev);

	 
	flush_work(&sl->tx_work);

	 
	spin_lock_bh(&sl->lock);
	tty->disc_data = NULL;
	sl->tty = NULL;
	spin_unlock_bh(&sl->lock);

	netdev_info(sl->dev, "slcan off %s.\n", tty->name);
	free_candev(sl->dev);
}

 
static int slcan_ioctl(struct tty_struct *tty, unsigned int cmd,
		       unsigned long arg)
{
	struct slcan *sl = tty->disc_data;
	unsigned int tmp;

	switch (cmd) {
	case SIOCGIFNAME:
		tmp = strlen(sl->dev->name) + 1;
		if (copy_to_user((void __user *)arg, sl->dev->name, tmp))
			return -EFAULT;
		return 0;

	case SIOCSIFHWADDR:
		return -EINVAL;

	default:
		return tty_mode_ioctl(tty, cmd, arg);
	}
}

static struct tty_ldisc_ops slcan_ldisc = {
	.owner		= THIS_MODULE,
	.num		= N_SLCAN,
	.name		= KBUILD_MODNAME,
	.open		= slcan_open,
	.close		= slcan_close,
	.ioctl		= slcan_ioctl,
	.receive_buf	= slcan_receive_buf,
	.write_wakeup	= slcan_write_wakeup,
};

static int __init slcan_init(void)
{
	int status;

	pr_info("serial line CAN interface driver\n");

	 
	status = tty_register_ldisc(&slcan_ldisc);
	if (status)
		pr_err("can't register line discipline\n");

	return status;
}

static void __exit slcan_exit(void)
{
	 
	tty_unregister_ldisc(&slcan_ldisc);
}

module_init(slcan_init);
module_exit(slcan_exit);
