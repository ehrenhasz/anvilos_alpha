
 
 

#include <linux/math.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/reboot.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include <linux/time64.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#ifndef MODULE
#include <linux/fs.h>
#endif

enum scmi_syspower_state {
	SCMI_SYSPOWER_IDLE,
	SCMI_SYSPOWER_IN_PROGRESS,
	SCMI_SYSPOWER_REBOOTING
};

 
struct scmi_syspower_conf {
	struct device *dev;
	enum scmi_syspower_state state;
	 
	struct mutex state_mtx;
	enum scmi_system_events required_transition;

	struct notifier_block userspace_nb;
	struct notifier_block reboot_nb;

	struct delayed_work forceful_work;
};

#define userspace_nb_to_sconf(x)	\
	container_of(x, struct scmi_syspower_conf, userspace_nb)

#define reboot_nb_to_sconf(x)		\
	container_of(x, struct scmi_syspower_conf, reboot_nb)

#define dwork_to_sconf(x)		\
	container_of(x, struct scmi_syspower_conf, forceful_work)

 
static int scmi_reboot_notifier(struct notifier_block *nb,
				unsigned long reason, void *__unused)
{
	struct scmi_syspower_conf *sc = reboot_nb_to_sconf(nb);

	mutex_lock(&sc->state_mtx);
	switch (reason) {
	case SYS_HALT:
	case SYS_POWER_OFF:
		if (sc->required_transition == SCMI_SYSTEM_SHUTDOWN)
			sc->state = SCMI_SYSPOWER_REBOOTING;
		break;
	case SYS_RESTART:
		if (sc->required_transition == SCMI_SYSTEM_COLDRESET ||
		    sc->required_transition == SCMI_SYSTEM_WARMRESET)
			sc->state = SCMI_SYSPOWER_REBOOTING;
		break;
	default:
		break;
	}

	if (sc->state == SCMI_SYSPOWER_REBOOTING) {
		dev_dbg(sc->dev, "Reboot in progress...cancel delayed work.\n");
		cancel_delayed_work_sync(&sc->forceful_work);
	}
	mutex_unlock(&sc->state_mtx);

	return NOTIFY_OK;
}

 
static inline void
scmi_request_forceful_transition(struct scmi_syspower_conf *sc)
{
	dev_dbg(sc->dev, "Serving forceful request:%d\n",
		sc->required_transition);

#ifndef MODULE
	emergency_sync();
#endif
	switch (sc->required_transition) {
	case SCMI_SYSTEM_SHUTDOWN:
		kernel_power_off();
		break;
	case SCMI_SYSTEM_COLDRESET:
	case SCMI_SYSTEM_WARMRESET:
		kernel_restart(NULL);
		break;
	default:
		break;
	}
}

static void scmi_forceful_work_func(struct work_struct *work)
{
	struct scmi_syspower_conf *sc;
	struct delayed_work *dwork;

	if (system_state > SYSTEM_RUNNING)
		return;

	dwork = to_delayed_work(work);
	sc = dwork_to_sconf(dwork);

	dev_dbg(sc->dev, "Graceful request timed out...forcing !\n");
	mutex_lock(&sc->state_mtx);
	 
	unregister_reboot_notifier(&sc->reboot_nb);
	if (sc->state == SCMI_SYSPOWER_IN_PROGRESS)
		scmi_request_forceful_transition(sc);
	mutex_unlock(&sc->state_mtx);
}

 
static void scmi_request_graceful_transition(struct scmi_syspower_conf *sc,
					     unsigned int timeout_ms)
{
	unsigned int adj_timeout_ms = 0;

	if (timeout_ms) {
		int ret;

		sc->reboot_nb.notifier_call = &scmi_reboot_notifier;
		ret = register_reboot_notifier(&sc->reboot_nb);
		if (!ret) {
			 
			adj_timeout_ms = mult_frac(timeout_ms, 3, 4);
			INIT_DELAYED_WORK(&sc->forceful_work,
					  scmi_forceful_work_func);
			schedule_delayed_work(&sc->forceful_work,
					      msecs_to_jiffies(adj_timeout_ms));
		} else {
			 
			dev_warn(sc->dev,
				 "Cannot register reboot notifier !\n");
		}
	}

	dev_dbg(sc->dev,
		"Serving graceful req:%d (timeout_ms:%u  adj_timeout_ms:%u)\n",
		sc->required_transition, timeout_ms, adj_timeout_ms);

	switch (sc->required_transition) {
	case SCMI_SYSTEM_SHUTDOWN:
		 
		orderly_poweroff(true);
		break;
	case SCMI_SYSTEM_COLDRESET:
	case SCMI_SYSTEM_WARMRESET:
		orderly_reboot();
		break;
	default:
		break;
	}
}

 
static int scmi_userspace_notifier(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct scmi_system_power_state_notifier_report *er = data;
	struct scmi_syspower_conf *sc = userspace_nb_to_sconf(nb);

	if (er->system_state >= SCMI_SYSTEM_POWERUP) {
		dev_err(sc->dev, "Ignoring unsupported system_state: 0x%X\n",
			er->system_state);
		return NOTIFY_DONE;
	}

	if (!SCMI_SYSPOWER_IS_REQUEST_GRACEFUL(er->flags)) {
		dev_err(sc->dev, "Ignoring forceful notification.\n");
		return NOTIFY_DONE;
	}

	 
	if (system_state > SYSTEM_RUNNING)
		return NOTIFY_DONE;
	mutex_lock(&sc->state_mtx);
	if (sc->state != SCMI_SYSPOWER_IDLE) {
		dev_dbg(sc->dev,
			"Transition already in progress...ignore.\n");
		mutex_unlock(&sc->state_mtx);
		return NOTIFY_DONE;
	}
	sc->state = SCMI_SYSPOWER_IN_PROGRESS;
	mutex_unlock(&sc->state_mtx);

	sc->required_transition = er->system_state;

	 
	dev_info(sc->dev, "Serving shutdown/reboot request: %d\n",
		 sc->required_transition);

	scmi_request_graceful_transition(sc, er->timeout);

	return NOTIFY_OK;
}

static int scmi_syspower_probe(struct scmi_device *sdev)
{
	int ret;
	struct scmi_syspower_conf *sc;
	struct scmi_handle *handle = sdev->handle;

	if (!handle)
		return -ENODEV;

	ret = handle->devm_protocol_acquire(sdev, SCMI_PROTOCOL_SYSTEM);
	if (ret)
		return ret;

	sc = devm_kzalloc(&sdev->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->state = SCMI_SYSPOWER_IDLE;
	mutex_init(&sc->state_mtx);
	sc->required_transition = SCMI_SYSTEM_MAX;
	sc->userspace_nb.notifier_call = &scmi_userspace_notifier;
	sc->dev = &sdev->dev;

	return handle->notify_ops->devm_event_notifier_register(sdev,
							   SCMI_PROTOCOL_SYSTEM,
					 SCMI_EVENT_SYSTEM_POWER_STATE_NOTIFIER,
						       NULL, &sc->userspace_nb);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_SYSTEM, "syspower" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_system_power_driver = {
	.name = "scmi-system-power",
	.probe = scmi_syspower_probe,
	.id_table = scmi_id_table,
};
module_scmi_driver(scmi_system_power_driver);

MODULE_AUTHOR("Cristian Marussi <cristian.marussi@arm.com>");
MODULE_DESCRIPTION("ARM SCMI SystemPower Control driver");
MODULE_LICENSE("GPL");
