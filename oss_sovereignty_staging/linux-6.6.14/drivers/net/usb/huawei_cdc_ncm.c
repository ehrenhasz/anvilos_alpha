
 

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/usb/usbnet.h>
#include <linux/usb/cdc-wdm.h>
#include <linux/usb/cdc_ncm.h>

 
struct huawei_cdc_ncm_state {
	struct cdc_ncm_ctx *ctx;
	atomic_t pmcount;
	struct usb_driver *subdriver;
	struct usb_interface *control;
	struct usb_interface *data;
};

static int huawei_cdc_ncm_manage_power(struct usbnet *usbnet_dev, int on)
{
	struct huawei_cdc_ncm_state *drvstate = (void *)&usbnet_dev->data;
	int rv;

	if ((on && atomic_add_return(1, &drvstate->pmcount) == 1) ||
			(!on && atomic_dec_and_test(&drvstate->pmcount))) {
		rv = usb_autopm_get_interface(usbnet_dev->intf);
		usbnet_dev->intf->needs_remote_wakeup = on;
		if (!rv)
			usb_autopm_put_interface(usbnet_dev->intf);
	}
	return 0;
}

static int huawei_cdc_ncm_wdm_manage_power(struct usb_interface *intf,
					   int status)
{
	struct usbnet *usbnet_dev = usb_get_intfdata(intf);

	 
	if (!usbnet_dev)
		return 0;

	return huawei_cdc_ncm_manage_power(usbnet_dev, status);
}


static int huawei_cdc_ncm_bind(struct usbnet *usbnet_dev,
			       struct usb_interface *intf)
{
	struct cdc_ncm_ctx *ctx;
	struct usb_driver *subdriver = ERR_PTR(-ENODEV);
	int ret;
	struct huawei_cdc_ncm_state *drvstate = (void *)&usbnet_dev->data;
	int drvflags = 0;

	 
	drvflags |= CDC_NCM_FLAG_NDP_TO_END;

	 
	drvflags |= CDC_NCM_FLAG_PREFER_NTB32;

	ret = cdc_ncm_bind_common(usbnet_dev, intf, 1, drvflags);
	if (ret)
		goto err;

	ctx = drvstate->ctx;

	if (usbnet_dev->status)
		 
		subdriver = usb_cdc_wdm_register(ctx->control,
						 &usbnet_dev->status->desc,
						 1024,  
						 WWAN_PORT_AT,
						 huawei_cdc_ncm_wdm_manage_power);
	if (IS_ERR(subdriver)) {
		ret = PTR_ERR(subdriver);
		cdc_ncm_unbind(usbnet_dev, intf);
		goto err;
	}

	 
	usbnet_dev->status = NULL;

	drvstate->subdriver = subdriver;

err:
	return ret;
}

static void huawei_cdc_ncm_unbind(struct usbnet *usbnet_dev,
				  struct usb_interface *intf)
{
	struct huawei_cdc_ncm_state *drvstate = (void *)&usbnet_dev->data;
	struct cdc_ncm_ctx *ctx = drvstate->ctx;

	if (drvstate->subdriver && drvstate->subdriver->disconnect)
		drvstate->subdriver->disconnect(ctx->control);
	drvstate->subdriver = NULL;

	cdc_ncm_unbind(usbnet_dev, intf);
}

static int huawei_cdc_ncm_suspend(struct usb_interface *intf,
				  pm_message_t message)
{
	int ret = 0;
	struct usbnet *usbnet_dev = usb_get_intfdata(intf);
	struct huawei_cdc_ncm_state *drvstate = (void *)&usbnet_dev->data;
	struct cdc_ncm_ctx *ctx = drvstate->ctx;

	if (ctx == NULL) {
		ret = -ENODEV;
		goto error;
	}

	ret = usbnet_suspend(intf, message);
	if (ret < 0)
		goto error;

	if (intf == ctx->control &&
		drvstate->subdriver &&
		drvstate->subdriver->suspend)
		ret = drvstate->subdriver->suspend(intf, message);
	if (ret < 0)
		usbnet_resume(intf);

error:
	return ret;
}

static int huawei_cdc_ncm_resume(struct usb_interface *intf)
{
	int ret = 0;
	struct usbnet *usbnet_dev = usb_get_intfdata(intf);
	struct huawei_cdc_ncm_state *drvstate = (void *)&usbnet_dev->data;
	bool callsub;
	struct cdc_ncm_ctx *ctx = drvstate->ctx;

	 
	callsub =
		(intf == ctx->control &&
		drvstate->subdriver &&
		drvstate->subdriver->resume);

	if (callsub)
		ret = drvstate->subdriver->resume(intf);
	if (ret < 0)
		goto err;
	ret = usbnet_resume(intf);
	if (ret < 0 && callsub)
		drvstate->subdriver->suspend(intf, PMSG_SUSPEND);
err:
	return ret;
}

static const struct driver_info huawei_cdc_ncm_info = {
	.description = "Huawei CDC NCM device",
	.flags = FLAG_NO_SETINT | FLAG_MULTI_PACKET | FLAG_WWAN,
	.bind = huawei_cdc_ncm_bind,
	.unbind = huawei_cdc_ncm_unbind,
	.manage_power = huawei_cdc_ncm_manage_power,
	.rx_fixup = cdc_ncm_rx_fixup,
	.tx_fixup = cdc_ncm_tx_fixup,
};

static const struct usb_device_id huawei_cdc_ncm_devs[] = {
	 
	{ USB_VENDOR_AND_INTERFACE_INFO(0x12d1, 0xff, 0x02, 0x16),
	  .driver_info = (unsigned long)&huawei_cdc_ncm_info,
	},
	{ USB_VENDOR_AND_INTERFACE_INFO(0x12d1, 0xff, 0x02, 0x46),
	  .driver_info = (unsigned long)&huawei_cdc_ncm_info,
	},
	{ USB_VENDOR_AND_INTERFACE_INFO(0x12d1, 0xff, 0x02, 0x76),
	  .driver_info = (unsigned long)&huawei_cdc_ncm_info,
	},
	{ USB_VENDOR_AND_INTERFACE_INFO(0x12d1, 0xff, 0x03, 0x16),
	  .driver_info = (unsigned long)&huawei_cdc_ncm_info,
	},

	 
	{
	},
};
MODULE_DEVICE_TABLE(usb, huawei_cdc_ncm_devs);

static struct usb_driver huawei_cdc_ncm_driver = {
	.name = "huawei_cdc_ncm",
	.id_table = huawei_cdc_ncm_devs,
	.probe = usbnet_probe,
	.disconnect = usbnet_disconnect,
	.suspend = huawei_cdc_ncm_suspend,
	.resume = huawei_cdc_ncm_resume,
	.reset_resume = huawei_cdc_ncm_resume,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};
module_usb_driver(huawei_cdc_ncm_driver);
MODULE_AUTHOR("Enrico Mioso <mrkiko.rs@gmail.com>");
MODULE_DESCRIPTION("USB CDC NCM host driver with encapsulated protocol support");
MODULE_LICENSE("GPL");
