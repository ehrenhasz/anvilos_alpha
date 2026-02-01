
 

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>

#include <net/nfc/nci.h>
#include <net/nfc/nci_core.h>

 
#define NCI_UART_SENDING	1
#define NCI_UART_TX_WAKEUP	2

static struct nci_uart *nci_uart_drivers[NCI_UART_DRIVER_MAX];

static inline struct sk_buff *nci_uart_dequeue(struct nci_uart *nu)
{
	struct sk_buff *skb = nu->tx_skb;

	if (!skb)
		skb = skb_dequeue(&nu->tx_q);
	else
		nu->tx_skb = NULL;

	return skb;
}

static inline int nci_uart_queue_empty(struct nci_uart *nu)
{
	if (nu->tx_skb)
		return 0;

	return skb_queue_empty(&nu->tx_q);
}

static int nci_uart_tx_wakeup(struct nci_uart *nu)
{
	if (test_and_set_bit(NCI_UART_SENDING, &nu->tx_state)) {
		set_bit(NCI_UART_TX_WAKEUP, &nu->tx_state);
		return 0;
	}

	schedule_work(&nu->write_work);

	return 0;
}

static void nci_uart_write_work(struct work_struct *work)
{
	struct nci_uart *nu = container_of(work, struct nci_uart, write_work);
	struct tty_struct *tty = nu->tty;
	struct sk_buff *skb;

restart:
	clear_bit(NCI_UART_TX_WAKEUP, &nu->tx_state);

	if (nu->ops.tx_start)
		nu->ops.tx_start(nu);

	while ((skb = nci_uart_dequeue(nu))) {
		int len;

		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		len = tty->ops->write(tty, skb->data, skb->len);
		skb_pull(skb, len);
		if (skb->len) {
			nu->tx_skb = skb;
			break;
		}
		kfree_skb(skb);
	}

	if (test_bit(NCI_UART_TX_WAKEUP, &nu->tx_state))
		goto restart;

	if (nu->ops.tx_done && nci_uart_queue_empty(nu))
		nu->ops.tx_done(nu);

	clear_bit(NCI_UART_SENDING, &nu->tx_state);
}

static int nci_uart_set_driver(struct tty_struct *tty, unsigned int driver)
{
	struct nci_uart *nu = NULL;
	int ret;

	if (driver >= NCI_UART_DRIVER_MAX)
		return -EINVAL;

	if (!nci_uart_drivers[driver])
		return -ENOENT;

	nu = kzalloc(sizeof(*nu), GFP_KERNEL);
	if (!nu)
		return -ENOMEM;

	memcpy(nu, nci_uart_drivers[driver], sizeof(struct nci_uart));
	nu->tty = tty;
	tty->disc_data = nu;
	skb_queue_head_init(&nu->tx_q);
	INIT_WORK(&nu->write_work, nci_uart_write_work);
	spin_lock_init(&nu->rx_lock);

	ret = nu->ops.open(nu);
	if (ret) {
		tty->disc_data = NULL;
		kfree(nu);
	} else if (!try_module_get(nu->owner)) {
		nu->ops.close(nu);
		tty->disc_data = NULL;
		kfree(nu);
		return -ENOENT;
	}
	return ret;
}

 

 
static int nci_uart_tty_open(struct tty_struct *tty)
{
	 
	if (!tty->ops->write)
		return -EOPNOTSUPP;

	tty->disc_data = NULL;
	tty->receive_room = 65536;

	 
	tty_driver_flush_buffer(tty);

	return 0;
}

 
static void nci_uart_tty_close(struct tty_struct *tty)
{
	struct nci_uart *nu = tty->disc_data;

	 
	tty->disc_data = NULL;

	if (!nu)
		return;

	kfree_skb(nu->tx_skb);
	kfree_skb(nu->rx_skb);

	skb_queue_purge(&nu->tx_q);

	nu->ops.close(nu);
	nu->tty = NULL;
	module_put(nu->owner);

	cancel_work_sync(&nu->write_work);

	kfree(nu);
}

 
static void nci_uart_tty_wakeup(struct tty_struct *tty)
{
	struct nci_uart *nu = tty->disc_data;

	if (!nu)
		return;

	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	if (tty != nu->tty)
		return;

	nci_uart_tx_wakeup(nu);
}

 
static int nci_uart_default_recv_buf(struct nci_uart *nu, const u8 *data,
				     int count)
{
	int chunk_len;

	if (!nu->ndev) {
		nfc_err(nu->tty->dev,
			"receive data from tty but no NCI dev is attached yet, drop buffer\n");
		return 0;
	}

	 
	while (count > 0) {
		 
		if (!nu->rx_skb) {
			nu->rx_packet_len = -1;
			nu->rx_skb = nci_skb_alloc(nu->ndev,
						   NCI_MAX_PACKET_SIZE,
						   GFP_ATOMIC);
			if (!nu->rx_skb)
				return -ENOMEM;
		}

		 
		if (nu->rx_skb->len < NCI_CTRL_HDR_SIZE) {
			skb_put_u8(nu->rx_skb, *data++);
			--count;
			continue;
		}

		 
		if (nu->rx_packet_len < 0)
			nu->rx_packet_len = NCI_CTRL_HDR_SIZE +
				nci_plen(nu->rx_skb->data);

		 
		chunk_len = nu->rx_packet_len - nu->rx_skb->len;
		if (count < chunk_len)
			chunk_len = count;
		skb_put_data(nu->rx_skb, data, chunk_len);
		data += chunk_len;
		count -= chunk_len;

		 
		if (nu->rx_packet_len == nu->rx_skb->len) {
			 
			if (nu->ops.recv(nu, nu->rx_skb) != 0)
				nfc_err(nu->tty->dev, "corrupted RX packet\n");
			 
			nu->rx_skb = NULL;
		}
	}

	return 0;
}

 
static void nci_uart_tty_receive(struct tty_struct *tty, const u8 *data,
				 const u8 *flags, size_t count)
{
	struct nci_uart *nu = tty->disc_data;

	if (!nu || tty != nu->tty)
		return;

	spin_lock(&nu->rx_lock);
	nci_uart_default_recv_buf(nu, data, count);
	spin_unlock(&nu->rx_lock);

	tty_unthrottle(tty);
}

 
static int nci_uart_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
			      unsigned long arg)
{
	struct nci_uart *nu = tty->disc_data;
	int err = 0;

	switch (cmd) {
	case NCIUARTSETDRIVER:
		if (!nu)
			return nci_uart_set_driver(tty, (unsigned int)arg);
		else
			return -EBUSY;
		break;
	default:
		err = n_tty_ioctl_helper(tty, cmd, arg);
		break;
	}

	return err;
}

 
static ssize_t nci_uart_tty_read(struct tty_struct *tty, struct file *file,
				 u8 *buf, size_t nr, void **cookie,
				 unsigned long offset)
{
	return 0;
}

static ssize_t nci_uart_tty_write(struct tty_struct *tty, struct file *file,
				  const u8 *data, size_t count)
{
	return 0;
}

static int nci_uart_send(struct nci_uart *nu, struct sk_buff *skb)
{
	 
	skb_queue_tail(&nu->tx_q, skb);

	 
	nci_uart_tx_wakeup(nu);

	return 0;
}

int nci_uart_register(struct nci_uart *nu)
{
	if (!nu || !nu->ops.open ||
	    !nu->ops.recv || !nu->ops.close)
		return -EINVAL;

	 
	nu->ops.send = nci_uart_send;

	 
	if (nci_uart_drivers[nu->driver]) {
		pr_err("driver %d is already registered\n", nu->driver);
		return -EBUSY;
	}
	nci_uart_drivers[nu->driver] = nu;

	pr_info("NCI uart driver '%s [%d]' registered\n", nu->name, nu->driver);

	return 0;
}
EXPORT_SYMBOL_GPL(nci_uart_register);

void nci_uart_unregister(struct nci_uart *nu)
{
	pr_info("NCI uart driver '%s [%d]' unregistered\n", nu->name,
		nu->driver);

	 
	nci_uart_drivers[nu->driver] = NULL;
}
EXPORT_SYMBOL_GPL(nci_uart_unregister);

void nci_uart_set_config(struct nci_uart *nu, int baudrate, int flow_ctrl)
{
	struct ktermios new_termios;

	if (!nu->tty)
		return;

	down_read(&nu->tty->termios_rwsem);
	new_termios = nu->tty->termios;
	up_read(&nu->tty->termios_rwsem);
	tty_termios_encode_baud_rate(&new_termios, baudrate, baudrate);

	if (flow_ctrl)
		new_termios.c_cflag |= CRTSCTS;
	else
		new_termios.c_cflag &= ~CRTSCTS;

	tty_set_termios(nu->tty, &new_termios);
}
EXPORT_SYMBOL_GPL(nci_uart_set_config);

static struct tty_ldisc_ops nci_uart_ldisc = {
	.owner		= THIS_MODULE,
	.num		= N_NCI,
	.name		= "n_nci",
	.open		= nci_uart_tty_open,
	.close		= nci_uart_tty_close,
	.read		= nci_uart_tty_read,
	.write		= nci_uart_tty_write,
	.receive_buf	= nci_uart_tty_receive,
	.write_wakeup	= nci_uart_tty_wakeup,
	.ioctl		= nci_uart_tty_ioctl,
	.compat_ioctl	= nci_uart_tty_ioctl,
};

static int __init nci_uart_init(void)
{
	return tty_register_ldisc(&nci_uart_ldisc);
}

static void __exit nci_uart_exit(void)
{
	tty_unregister_ldisc(&nci_uart_ldisc);
}

module_init(nci_uart_init);
module_exit(nci_uart_exit);

MODULE_AUTHOR("Marvell International Ltd.");
MODULE_DESCRIPTION("NFC NCI UART driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_NCI);
