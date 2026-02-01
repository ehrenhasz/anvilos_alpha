
 

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/usb.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>

#include <sound/rawmidi.h>
#include "usx2y.h"
#include "usbusx2y.h"
#include "usX2Yhwdep.h"

MODULE_AUTHOR("Karsten Wiese <annabellesgarden@yahoo.de>");
MODULE_DESCRIPTION("TASCAM "NAME_ALLCAPS" Version 0.8.7.2");
MODULE_LICENSE("GPL");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;  
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;  
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;  

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for "NAME_ALLCAPS".");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for "NAME_ALLCAPS".");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable "NAME_ALLCAPS".");

static int snd_usx2y_card_used[SNDRV_CARDS];

static void snd_usx2y_card_private_free(struct snd_card *card);
static void usx2y_unlinkseq(struct snd_usx2y_async_seq *s);

 
static void i_usx2y_out04_int(struct urb *urb)
{
#ifdef CONFIG_SND_DEBUG
	if (urb->status) {
		int i;
		struct usx2ydev *usx2y = urb->context;

		for (i = 0; i < 10 && usx2y->as04.urb[i] != urb; i++)
			;
		snd_printdd("%s urb %i status=%i\n", __func__, i, urb->status);
	}
#endif
}

static void i_usx2y_in04_int(struct urb *urb)
{
	int			err = 0;
	struct usx2ydev		*usx2y = urb->context;
	struct us428ctls_sharedmem	*us428ctls = usx2y->us428ctls_sharedmem;
	struct us428_p4out *p4out;
	int i, j, n, diff, send;

	usx2y->in04_int_calls++;

	if (urb->status) {
		snd_printdd("Interrupt Pipe 4 came back with status=%i\n", urb->status);
		return;
	}

	
	if (us428ctls) {
		diff = -1;
		if (us428ctls->ctl_snapshot_last == -2) {
			diff = 0;
			memcpy(usx2y->in04_last, usx2y->in04_buf, sizeof(usx2y->in04_last));
			us428ctls->ctl_snapshot_last = -1;
		} else {
			for (i = 0; i < 21; i++) {
				if (usx2y->in04_last[i] != ((char *)usx2y->in04_buf)[i]) {
					if (diff < 0)
						diff = i;
					usx2y->in04_last[i] = ((char *)usx2y->in04_buf)[i];
				}
			}
		}
		if (diff >= 0) {
			n = us428ctls->ctl_snapshot_last + 1;
			if (n >= N_US428_CTL_BUFS || n < 0)
				n = 0;
			memcpy(us428ctls->ctl_snapshot + n, usx2y->in04_buf, sizeof(us428ctls->ctl_snapshot[0]));
			us428ctls->ctl_snapshot_differs_at[n] = diff;
			us428ctls->ctl_snapshot_last = n;
			wake_up(&usx2y->us428ctls_wait_queue_head);
		}
	}

	if (usx2y->us04) {
		if (!usx2y->us04->submitted) {
			do {
				err = usb_submit_urb(usx2y->us04->urb[usx2y->us04->submitted++], GFP_ATOMIC);
			} while (!err && usx2y->us04->submitted < usx2y->us04->len);
		}
	} else {
		if (us428ctls && us428ctls->p4out_last >= 0 && us428ctls->p4out_last < N_US428_P4OUT_BUFS) {
			if (us428ctls->p4out_last != us428ctls->p4out_sent) {
				send = us428ctls->p4out_sent + 1;
				if (send >= N_US428_P4OUT_BUFS)
					send = 0;
				for (j = 0; j < URBS_ASYNC_SEQ && !err; ++j) {
					if (!usx2y->as04.urb[j]->status) {
						p4out = us428ctls->p4out + send;	
						usb_fill_bulk_urb(usx2y->as04.urb[j], usx2y->dev,
								  usb_sndbulkpipe(usx2y->dev, 0x04), &p4out->val.vol,
								  p4out->type == ELT_LIGHT ? sizeof(struct us428_lights) : 5,
								  i_usx2y_out04_int, usx2y);
						err = usb_submit_urb(usx2y->as04.urb[j], GFP_ATOMIC);
						us428ctls->p4out_sent = send;
						break;
					}
				}
			}
		}
	}

	if (err)
		snd_printk(KERN_ERR "in04_int() usb_submit_urb err=%i\n", err);

	urb->dev = usx2y->dev;
	usb_submit_urb(urb, GFP_ATOMIC);
}

 
int usx2y_async_seq04_init(struct usx2ydev *usx2y)
{
	int	err = 0, i;

	if (WARN_ON(usx2y->as04.buffer))
		return -EBUSY;

	usx2y->as04.buffer = kmalloc_array(URBS_ASYNC_SEQ,
					   URB_DATA_LEN_ASYNC_SEQ, GFP_KERNEL);
	if (!usx2y->as04.buffer) {
		err = -ENOMEM;
	} else {
		for (i = 0; i < URBS_ASYNC_SEQ; ++i) {
			usx2y->as04.urb[i] = usb_alloc_urb(0, GFP_KERNEL);
			if (!usx2y->as04.urb[i]) {
				err = -ENOMEM;
				break;
			}
			usb_fill_bulk_urb(usx2y->as04.urb[i], usx2y->dev,
					  usb_sndbulkpipe(usx2y->dev, 0x04),
					  usx2y->as04.buffer + URB_DATA_LEN_ASYNC_SEQ * i, 0,
					  i_usx2y_out04_int, usx2y);
			err = usb_urb_ep_type_check(usx2y->as04.urb[i]);
			if (err < 0)
				break;
		}
	}
	if (err)
		usx2y_unlinkseq(&usx2y->as04);
	return err;
}

int usx2y_in04_init(struct usx2ydev *usx2y)
{
	int err;

	if (WARN_ON(usx2y->in04_urb))
		return -EBUSY;

	usx2y->in04_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!usx2y->in04_urb) {
		err = -ENOMEM;
		goto error;
	}

	usx2y->in04_buf = kmalloc(21, GFP_KERNEL);
	if (!usx2y->in04_buf) {
		err = -ENOMEM;
		goto error;
	}

	init_waitqueue_head(&usx2y->in04_wait_queue);
	usb_fill_int_urb(usx2y->in04_urb, usx2y->dev, usb_rcvintpipe(usx2y->dev, 0x4),
			 usx2y->in04_buf, 21,
			 i_usx2y_in04_int, usx2y,
			 10);
	if (usb_urb_ep_type_check(usx2y->in04_urb)) {
		err = -EINVAL;
		goto error;
	}
	return usb_submit_urb(usx2y->in04_urb, GFP_KERNEL);

 error:
	kfree(usx2y->in04_buf);
	usb_free_urb(usx2y->in04_urb);
	usx2y->in04_buf = NULL;
	usx2y->in04_urb = NULL;
	return err;
}

static void usx2y_unlinkseq(struct snd_usx2y_async_seq *s)
{
	int	i;

	for (i = 0; i < URBS_ASYNC_SEQ; ++i) {
		if (!s->urb[i])
			continue;
		usb_kill_urb(s->urb[i]);
		usb_free_urb(s->urb[i]);
		s->urb[i] = NULL;
	}
	kfree(s->buffer);
	s->buffer = NULL;
}

static const struct usb_device_id snd_usx2y_usb_id_table[] = {
	{
		.match_flags =	USB_DEVICE_ID_MATCH_DEVICE,
		.idVendor =	0x1604,
		.idProduct =	USB_ID_US428
	},
	{
		.match_flags =	USB_DEVICE_ID_MATCH_DEVICE,
		.idVendor =	0x1604,
		.idProduct =	USB_ID_US122
	},
	{
		.match_flags =	USB_DEVICE_ID_MATCH_DEVICE,
		.idVendor =	0x1604,
		.idProduct =	USB_ID_US224
	},
	{   }
};
MODULE_DEVICE_TABLE(usb, snd_usx2y_usb_id_table);

static int usx2y_create_card(struct usb_device *device,
			     struct usb_interface *intf,
			     struct snd_card **cardp)
{
	int		dev;
	struct snd_card *card;
	int err;

	for (dev = 0; dev < SNDRV_CARDS; ++dev)
		if (enable[dev] && !snd_usx2y_card_used[dev])
			break;
	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	err = snd_card_new(&intf->dev, index[dev], id[dev], THIS_MODULE,
			   sizeof(struct usx2ydev), &card);
	if (err < 0)
		return err;
	snd_usx2y_card_used[usx2y(card)->card_index = dev] = 1;
	card->private_free = snd_usx2y_card_private_free;
	usx2y(card)->dev = device;
	init_waitqueue_head(&usx2y(card)->prepare_wait_queue);
	init_waitqueue_head(&usx2y(card)->us428ctls_wait_queue_head);
	mutex_init(&usx2y(card)->pcm_mutex);
	INIT_LIST_HEAD(&usx2y(card)->midi_list);
	strcpy(card->driver, "USB "NAME_ALLCAPS"");
	sprintf(card->shortname, "TASCAM "NAME_ALLCAPS"");
	sprintf(card->longname, "%s (%x:%x if %d at %03d/%03d)",
		card->shortname,
		le16_to_cpu(device->descriptor.idVendor),
		le16_to_cpu(device->descriptor.idProduct),
		0,
		usx2y(card)->dev->bus->busnum, usx2y(card)->dev->devnum);
	*cardp = card;
	return 0;
}

static void snd_usx2y_card_private_free(struct snd_card *card)
{
	struct usx2ydev *usx2y = usx2y(card);

	kfree(usx2y->in04_buf);
	usb_free_urb(usx2y->in04_urb);
	if (usx2y->us428ctls_sharedmem)
		free_pages_exact(usx2y->us428ctls_sharedmem,
				 US428_SHAREDMEM_PAGES);
	if (usx2y->card_index >= 0 && usx2y->card_index < SNDRV_CARDS)
		snd_usx2y_card_used[usx2y->card_index] = 0;
}

static void snd_usx2y_disconnect(struct usb_interface *intf)
{
	struct snd_card *card;
	struct usx2ydev *usx2y;
	struct list_head *p;

	card = usb_get_intfdata(intf);
	if (!card)
		return;
	usx2y = usx2y(card);
	usx2y->chip_status = USX2Y_STAT_CHIP_HUP;
	usx2y_unlinkseq(&usx2y->as04);
	usb_kill_urb(usx2y->in04_urb);
	snd_card_disconnect(card);

	 
	list_for_each(p, &usx2y->midi_list) {
		snd_usbmidi_disconnect(p);
	}
	if (usx2y->us428ctls_sharedmem)
		wake_up(&usx2y->us428ctls_wait_queue_head);
	snd_card_free(card);
}

static int snd_usx2y_probe(struct usb_interface *intf,
			   const struct usb_device_id *id)
{
	struct usb_device *device = interface_to_usbdev(intf);
	struct snd_card *card;
	int err;

	if (le16_to_cpu(device->descriptor.idVendor) != 0x1604 ||
	    (le16_to_cpu(device->descriptor.idProduct) != USB_ID_US122 &&
	     le16_to_cpu(device->descriptor.idProduct) != USB_ID_US224 &&
	     le16_to_cpu(device->descriptor.idProduct) != USB_ID_US428))
		return -EINVAL;

	err = usx2y_create_card(device, intf, &card);
	if (err < 0)
		return err;
	err = usx2y_hwdep_new(card, device);
	if (err < 0)
		goto error;
	err = snd_card_register(card);
	if (err < 0)
		goto error;

	dev_set_drvdata(&intf->dev, card);
	return 0;

 error:
	snd_card_free(card);
	return err;
}

static struct usb_driver snd_usx2y_usb_driver = {
	.name =		"snd-usb-usx2y",
	.probe =	snd_usx2y_probe,
	.disconnect =	snd_usx2y_disconnect,
	.id_table =	snd_usx2y_usb_id_table,
};
module_usb_driver(snd_usx2y_usb_driver);
