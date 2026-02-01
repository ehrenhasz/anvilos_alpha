
 

#include <linux/hid.h>
#include <linux/hid-debug.h>
#include <linux/input.h>
#include "hid-ids.h"

#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/backlight.h>
#include <linux/lcd.h>

#include <linux/leds.h>

#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include <linux/completion.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <media/rc-core.h>

#include "hid-picolcd.h"


int picolcd_raw_cir(struct picolcd_data *data,
		struct hid_report *report, u8 *raw_data, int size)
{
	unsigned long flags;
	int i, w, sz;
	struct ir_raw_event rawir = {};

	 
	spin_lock_irqsave(&data->lock, flags);
	if (!data->rc_dev || (data->status & PICOLCD_CIR_SHUN)) {
		spin_unlock_irqrestore(&data->lock, flags);
		return 1;
	}
	spin_unlock_irqrestore(&data->lock, flags);

	 
	sz = size > 0 ? min((int)raw_data[0], size-1) : 0;
	for (i = 0; i+1 < sz; i += 2) {
		w = (raw_data[i] << 8) | (raw_data[i+1]);
		rawir.pulse = !!(w & 0x8000);
		rawir.duration = rawir.pulse ? (65536 - w) : w;
		 
		if (i == 0 && rawir.duration > 15000)
			rawir.duration -= 15000;
		ir_raw_event_store(data->rc_dev, &rawir);
	}
	ir_raw_event_handle(data->rc_dev);

	return 1;
}

static int picolcd_cir_open(struct rc_dev *dev)
{
	struct picolcd_data *data = dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	data->status &= ~PICOLCD_CIR_SHUN;
	spin_unlock_irqrestore(&data->lock, flags);
	return 0;
}

static void picolcd_cir_close(struct rc_dev *dev)
{
	struct picolcd_data *data = dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	data->status |= PICOLCD_CIR_SHUN;
	spin_unlock_irqrestore(&data->lock, flags);
}

 
int picolcd_init_cir(struct picolcd_data *data, struct hid_report *report)
{
	struct rc_dev *rdev;
	int ret = 0;

	rdev = rc_allocate_device(RC_DRIVER_IR_RAW);
	if (!rdev)
		return -ENOMEM;

	rdev->priv             = data;
	rdev->allowed_protocols = RC_PROTO_BIT_ALL_IR_DECODER;
	rdev->open             = picolcd_cir_open;
	rdev->close            = picolcd_cir_close;
	rdev->device_name      = data->hdev->name;
	rdev->input_phys       = data->hdev->phys;
	rdev->input_id.bustype = data->hdev->bus;
	rdev->input_id.vendor  = data->hdev->vendor;
	rdev->input_id.product = data->hdev->product;
	rdev->input_id.version = data->hdev->version;
	rdev->dev.parent       = &data->hdev->dev;
	rdev->driver_name      = PICOLCD_NAME;
	rdev->map_name         = RC_MAP_RC6_MCE;
	rdev->timeout          = MS_TO_US(100);
	rdev->rx_resolution    = 1;

	ret = rc_register_device(rdev);
	if (ret)
		goto err;
	data->rc_dev = rdev;
	return 0;

err:
	rc_free_device(rdev);
	return ret;
}

void picolcd_exit_cir(struct picolcd_data *data)
{
	struct rc_dev *rdev = data->rc_dev;

	data->rc_dev = NULL;
	rc_unregister_device(rdev);
}

