
 

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/slab.h>
#include <linux/suspend.h>

#include "cros_ec.h"

static struct cros_ec_platform ec_p = {
	.ec_name = CROS_EC_DEV_NAME,
	.cmd_offset = EC_CMD_PASSTHRU_OFFSET(CROS_EC_DEV_EC_INDEX),
};

static struct cros_ec_platform pd_p = {
	.ec_name = CROS_EC_DEV_PD_NAME,
	.cmd_offset = EC_CMD_PASSTHRU_OFFSET(CROS_EC_DEV_PD_INDEX),
};

 
static irqreturn_t cros_ec_irq_handler(int irq, void *data)
{
	struct cros_ec_device *ec_dev = data;

	ec_dev->last_event_time = cros_ec_get_time_ns();

	return IRQ_WAKE_THREAD;
}

 
static bool cros_ec_handle_event(struct cros_ec_device *ec_dev)
{
	bool wake_event;
	bool ec_has_more_events;
	int ret;

	ret = cros_ec_get_next_event(ec_dev, &wake_event, &ec_has_more_events);

	 
	if (wake_event && device_may_wakeup(ec_dev->dev))
		pm_wakeup_event(ec_dev->dev, 0);

	if (ret > 0)
		blocking_notifier_call_chain(&ec_dev->event_notifier,
					     0, ec_dev);

	return ec_has_more_events;
}

 
irqreturn_t cros_ec_irq_thread(int irq, void *data)
{
	struct cros_ec_device *ec_dev = data;
	bool ec_has_more_events;

	do {
		ec_has_more_events = cros_ec_handle_event(ec_dev);
	} while (ec_has_more_events);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL(cros_ec_irq_thread);

static int cros_ec_sleep_event(struct cros_ec_device *ec_dev, u8 sleep_event)
{
	int ret;
	struct {
		struct cros_ec_command msg;
		union {
			struct ec_params_host_sleep_event req0;
			struct ec_params_host_sleep_event_v1 req1;
			struct ec_response_host_sleep_event_v1 resp1;
		} u;
	} __packed buf;

	memset(&buf, 0, sizeof(buf));

	if (ec_dev->host_sleep_v1) {
		buf.u.req1.sleep_event = sleep_event;
		buf.u.req1.suspend_params.sleep_timeout_ms =
				ec_dev->suspend_timeout_ms;

		buf.msg.outsize = sizeof(buf.u.req1);
		if ((sleep_event == HOST_SLEEP_EVENT_S3_RESUME) ||
		    (sleep_event == HOST_SLEEP_EVENT_S0IX_RESUME))
			buf.msg.insize = sizeof(buf.u.resp1);

		buf.msg.version = 1;

	} else {
		buf.u.req0.sleep_event = sleep_event;
		buf.msg.outsize = sizeof(buf.u.req0);
	}

	buf.msg.command = EC_CMD_HOST_SLEEP_EVENT;

	ret = cros_ec_cmd_xfer_status(ec_dev, &buf.msg);
	 
	if (ret >= 0 && ec_dev->host_sleep_v1 &&
	    (sleep_event == HOST_SLEEP_EVENT_S0IX_RESUME ||
	     sleep_event == HOST_SLEEP_EVENT_S3_RESUME)) {
		ec_dev->last_resume_result =
			buf.u.resp1.resume_response.sleep_transitions;

		WARN_ONCE(buf.u.resp1.resume_response.sleep_transitions &
			  EC_HOST_RESUME_SLEEP_TIMEOUT,
			  "EC detected sleep transition timeout. Total sleep transitions: %d",
			  buf.u.resp1.resume_response.sleep_transitions &
			  EC_HOST_RESUME_SLEEP_TRANSITIONS_MASK);
	}

	return ret;
}

static int cros_ec_ready_event(struct notifier_block *nb,
			       unsigned long queued_during_suspend,
			       void *_notify)
{
	struct cros_ec_device *ec_dev = container_of(nb, struct cros_ec_device,
						     notifier_ready);
	u32 host_event = cros_ec_get_host_event(ec_dev);

	if (host_event & EC_HOST_EVENT_MASK(EC_HOST_EVENT_INTERFACE_READY)) {
		mutex_lock(&ec_dev->lock);
		cros_ec_query_all(ec_dev);
		mutex_unlock(&ec_dev->lock);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

 
int cros_ec_register(struct cros_ec_device *ec_dev)
{
	struct device *dev = ec_dev->dev;
	int err = 0;

	BLOCKING_INIT_NOTIFIER_HEAD(&ec_dev->event_notifier);
	BLOCKING_INIT_NOTIFIER_HEAD(&ec_dev->panic_notifier);

	ec_dev->max_request = sizeof(struct ec_params_hello);
	ec_dev->max_response = sizeof(struct ec_response_get_protocol_info);
	ec_dev->max_passthru = 0;
	ec_dev->ec = NULL;
	ec_dev->pd = NULL;
	ec_dev->suspend_timeout_ms = EC_HOST_SLEEP_TIMEOUT_DEFAULT;

	ec_dev->din = devm_kzalloc(dev, ec_dev->din_size, GFP_KERNEL);
	if (!ec_dev->din)
		return -ENOMEM;

	ec_dev->dout = devm_kzalloc(dev, ec_dev->dout_size, GFP_KERNEL);
	if (!ec_dev->dout)
		return -ENOMEM;

	lockdep_register_key(&ec_dev->lockdep_key);
	mutex_init(&ec_dev->lock);
	lockdep_set_class(&ec_dev->lock, &ec_dev->lockdep_key);

	err = cros_ec_query_all(ec_dev);
	if (err) {
		dev_err(dev, "Cannot identify the EC: error %d\n", err);
		goto exit;
	}

	if (ec_dev->irq > 0) {
		err = devm_request_threaded_irq(dev, ec_dev->irq,
						cros_ec_irq_handler,
						cros_ec_irq_thread,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						"chromeos-ec", ec_dev);
		if (err) {
			dev_err(dev, "Failed to request IRQ %d: %d\n",
				ec_dev->irq, err);
			goto exit;
		}
	}

	 
	ec_dev->ec = platform_device_register_data(ec_dev->dev, "cros-ec-dev",
					PLATFORM_DEVID_AUTO, &ec_p,
					sizeof(struct cros_ec_platform));
	if (IS_ERR(ec_dev->ec)) {
		dev_err(ec_dev->dev,
			"Failed to create CrOS EC platform device\n");
		err = PTR_ERR(ec_dev->ec);
		goto exit;
	}

	if (ec_dev->max_passthru) {
		 
		ec_dev->pd = platform_device_register_data(ec_dev->dev,
					"cros-ec-dev",
					PLATFORM_DEVID_AUTO, &pd_p,
					sizeof(struct cros_ec_platform));
		if (IS_ERR(ec_dev->pd)) {
			dev_err(ec_dev->dev,
				"Failed to create CrOS PD platform device\n");
			err = PTR_ERR(ec_dev->pd);
			goto exit;
		}
	}

	if (IS_ENABLED(CONFIG_OF) && dev->of_node) {
		err = devm_of_platform_populate(dev);
		if (err) {
			dev_err(dev, "Failed to register sub-devices\n");
			goto exit;
		}
	}

	 
	err = cros_ec_sleep_event(ec_dev, 0);
	if (err < 0)
		dev_dbg(ec_dev->dev, "Error %d clearing sleep event to ec\n",
			err);

	if (ec_dev->mkbp_event_supported) {
		 
		ec_dev->notifier_ready.notifier_call = cros_ec_ready_event;
		err = blocking_notifier_chain_register(&ec_dev->event_notifier,
						      &ec_dev->notifier_ready);
		if (err)
			goto exit;
	}

	dev_info(dev, "Chrome EC device registered\n");

	 
	if (ec_dev->mkbp_event_supported)
		cros_ec_irq_thread(0, ec_dev);

	return 0;
exit:
	platform_device_unregister(ec_dev->ec);
	platform_device_unregister(ec_dev->pd);
	mutex_destroy(&ec_dev->lock);
	lockdep_unregister_key(&ec_dev->lockdep_key);
	return err;
}
EXPORT_SYMBOL(cros_ec_register);

 
void cros_ec_unregister(struct cros_ec_device *ec_dev)
{
	platform_device_unregister(ec_dev->pd);
	platform_device_unregister(ec_dev->ec);
	mutex_destroy(&ec_dev->lock);
	lockdep_unregister_key(&ec_dev->lockdep_key);
}
EXPORT_SYMBOL(cros_ec_unregister);

#ifdef CONFIG_PM_SLEEP
static void cros_ec_send_suspend_event(struct cros_ec_device *ec_dev)
{
	int ret;
	u8 sleep_event;

	sleep_event = (!IS_ENABLED(CONFIG_ACPI) || pm_suspend_via_firmware()) ?
		      HOST_SLEEP_EVENT_S3_SUSPEND :
		      HOST_SLEEP_EVENT_S0IX_SUSPEND;

	ret = cros_ec_sleep_event(ec_dev, sleep_event);
	if (ret < 0)
		dev_dbg(ec_dev->dev, "Error %d sending suspend event to ec\n",
			ret);
}

 
int cros_ec_suspend_prepare(struct cros_ec_device *ec_dev)
{
	cros_ec_send_suspend_event(ec_dev);
	return 0;
}
EXPORT_SYMBOL(cros_ec_suspend_prepare);

static void cros_ec_disable_irq(struct cros_ec_device *ec_dev)
{
	struct device *dev = ec_dev->dev;
	if (device_may_wakeup(dev))
		ec_dev->wake_enabled = !enable_irq_wake(ec_dev->irq);
	else
		ec_dev->wake_enabled = false;

	disable_irq(ec_dev->irq);
	ec_dev->suspended = true;
}

 
int cros_ec_suspend_late(struct cros_ec_device *ec_dev)
{
	cros_ec_disable_irq(ec_dev);
	return 0;
}
EXPORT_SYMBOL(cros_ec_suspend_late);

 
int cros_ec_suspend(struct cros_ec_device *ec_dev)
{
	cros_ec_send_suspend_event(ec_dev);
	cros_ec_disable_irq(ec_dev);
	return 0;
}
EXPORT_SYMBOL(cros_ec_suspend);

static void cros_ec_report_events_during_suspend(struct cros_ec_device *ec_dev)
{
	bool wake_event;

	while (ec_dev->mkbp_event_supported &&
	       cros_ec_get_next_event(ec_dev, &wake_event, NULL) > 0) {
		blocking_notifier_call_chain(&ec_dev->event_notifier,
					     1, ec_dev);

		if (wake_event && device_may_wakeup(ec_dev->dev))
			pm_wakeup_event(ec_dev->dev, 0);
	}
}

static void cros_ec_send_resume_event(struct cros_ec_device *ec_dev)
{
	int ret;
	u8 sleep_event;

	sleep_event = (!IS_ENABLED(CONFIG_ACPI) || pm_suspend_via_firmware()) ?
		      HOST_SLEEP_EVENT_S3_RESUME :
		      HOST_SLEEP_EVENT_S0IX_RESUME;

	ret = cros_ec_sleep_event(ec_dev, sleep_event);
	if (ret < 0)
		dev_dbg(ec_dev->dev, "Error %d sending resume event to ec\n",
			ret);
}

 
void cros_ec_resume_complete(struct cros_ec_device *ec_dev)
{
	cros_ec_send_resume_event(ec_dev);
}
EXPORT_SYMBOL(cros_ec_resume_complete);

static void cros_ec_enable_irq(struct cros_ec_device *ec_dev)
{
	ec_dev->suspended = false;
	enable_irq(ec_dev->irq);

	if (ec_dev->wake_enabled)
		disable_irq_wake(ec_dev->irq);

	 
	cros_ec_report_events_during_suspend(ec_dev);
}

 
int cros_ec_resume_early(struct cros_ec_device *ec_dev)
{
	cros_ec_enable_irq(ec_dev);
	return 0;
}
EXPORT_SYMBOL(cros_ec_resume_early);

 
int cros_ec_resume(struct cros_ec_device *ec_dev)
{
	cros_ec_enable_irq(ec_dev);
	cros_ec_send_resume_event(ec_dev);
	return 0;
}
EXPORT_SYMBOL(cros_ec_resume);

#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ChromeOS EC core driver");
