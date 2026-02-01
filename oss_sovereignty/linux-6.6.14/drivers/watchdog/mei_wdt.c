
 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/completion.h>
#include <linux/watchdog.h>

#include <linux/uuid.h>
#include <linux/mei_cl_bus.h>

 
#define INTEL_AMT_WATCHDOG_ID "iamt_wdt"

#define MEI_WDT_DEFAULT_TIMEOUT   120   
#define MEI_WDT_MIN_TIMEOUT       120   
#define MEI_WDT_MAX_TIMEOUT     65535   

 
#define MEI_MANAGEMENT_CONTROL 0x02

 
#define MEI_MC_VERSION_NUMBER  0x10

 
#define MEI_MC_START_WD_TIMER_REQ  0x13
#define MEI_MC_START_WD_TIMER_RES  0x83
#define   MEI_WDT_STATUS_SUCCESS 0
#define   MEI_WDT_WDSTATE_NOT_REQUIRED 0x1
#define MEI_MC_STOP_WD_TIMER_REQ   0x14

 
enum mei_wdt_state {
	MEI_WDT_PROBE,
	MEI_WDT_IDLE,
	MEI_WDT_START,
	MEI_WDT_RUNNING,
	MEI_WDT_STOPPING,
	MEI_WDT_NOT_REQUIRED,
};

static const char *mei_wdt_state_str(enum mei_wdt_state state)
{
	switch (state) {
	case MEI_WDT_PROBE:
		return "PROBE";
	case MEI_WDT_IDLE:
		return "IDLE";
	case MEI_WDT_START:
		return "START";
	case MEI_WDT_RUNNING:
		return "RUNNING";
	case MEI_WDT_STOPPING:
		return "STOPPING";
	case MEI_WDT_NOT_REQUIRED:
		return "NOT_REQUIRED";
	default:
		return "unknown";
	}
}

 
struct mei_wdt {
	struct watchdog_device wdd;

	struct mei_cl_device *cldev;
	enum mei_wdt_state state;
	bool resp_required;
	struct completion response;
	struct work_struct unregister;
	struct mutex reg_lock;
	u16 timeout;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs_dir;
#endif  
};

 
struct mei_mc_hdr {
	u8 command;
	u8 bytecount;
	u8 subcommand;
	u8 versionnumber;
};

 
struct mei_wdt_start_request {
	struct mei_mc_hdr hdr;
	u16 timeout;
	u8 reserved[17];
} __packed;

 
struct mei_wdt_start_response {
	struct mei_mc_hdr hdr;
	u8 status;
	u8 wdstate;
} __packed;

 
struct mei_wdt_stop_request {
	struct mei_mc_hdr hdr;
} __packed;

 
static int mei_wdt_ping(struct mei_wdt *wdt)
{
	struct mei_wdt_start_request req;
	const size_t req_len = sizeof(req);
	int ret;

	memset(&req, 0, req_len);
	req.hdr.command = MEI_MANAGEMENT_CONTROL;
	req.hdr.bytecount = req_len - offsetof(struct mei_mc_hdr, subcommand);
	req.hdr.subcommand = MEI_MC_START_WD_TIMER_REQ;
	req.hdr.versionnumber = MEI_MC_VERSION_NUMBER;
	req.timeout = wdt->timeout;

	ret = mei_cldev_send(wdt->cldev, (u8 *)&req, req_len);
	if (ret < 0)
		return ret;

	return 0;
}

 
static int mei_wdt_stop(struct mei_wdt *wdt)
{
	struct mei_wdt_stop_request req;
	const size_t req_len = sizeof(req);
	int ret;

	memset(&req, 0, req_len);
	req.hdr.command = MEI_MANAGEMENT_CONTROL;
	req.hdr.bytecount = req_len - offsetof(struct mei_mc_hdr, subcommand);
	req.hdr.subcommand = MEI_MC_STOP_WD_TIMER_REQ;
	req.hdr.versionnumber = MEI_MC_VERSION_NUMBER;

	ret = mei_cldev_send(wdt->cldev, (u8 *)&req, req_len);
	if (ret < 0)
		return ret;

	return 0;
}

 
static int mei_wdt_ops_start(struct watchdog_device *wdd)
{
	struct mei_wdt *wdt = watchdog_get_drvdata(wdd);

	wdt->state = MEI_WDT_START;
	wdd->timeout = wdt->timeout;
	return 0;
}

 
static int mei_wdt_ops_stop(struct watchdog_device *wdd)
{
	struct mei_wdt *wdt = watchdog_get_drvdata(wdd);
	int ret;

	if (wdt->state != MEI_WDT_RUNNING)
		return 0;

	wdt->state = MEI_WDT_STOPPING;

	ret = mei_wdt_stop(wdt);
	if (ret)
		return ret;

	wdt->state = MEI_WDT_IDLE;

	return 0;
}

 
static int mei_wdt_ops_ping(struct watchdog_device *wdd)
{
	struct mei_wdt *wdt = watchdog_get_drvdata(wdd);
	int ret;

	if (wdt->state != MEI_WDT_START && wdt->state != MEI_WDT_RUNNING)
		return 0;

	if (wdt->resp_required)
		init_completion(&wdt->response);

	wdt->state = MEI_WDT_RUNNING;
	ret = mei_wdt_ping(wdt);
	if (ret)
		return ret;

	if (wdt->resp_required)
		ret = wait_for_completion_killable(&wdt->response);

	return ret;
}

 
static int mei_wdt_ops_set_timeout(struct watchdog_device *wdd,
				   unsigned int timeout)
{

	struct mei_wdt *wdt = watchdog_get_drvdata(wdd);

	 
	wdt->timeout = timeout;
	wdd->timeout = timeout;

	return 0;
}

static const struct watchdog_ops wd_ops = {
	.owner       = THIS_MODULE,
	.start       = mei_wdt_ops_start,
	.stop        = mei_wdt_ops_stop,
	.ping        = mei_wdt_ops_ping,
	.set_timeout = mei_wdt_ops_set_timeout,
};

 
static struct watchdog_info wd_info = {
	.identity = INTEL_AMT_WATCHDOG_ID,
	.options  = WDIOF_KEEPALIVEPING |
		    WDIOF_SETTIMEOUT |
		    WDIOF_ALARMONLY,
};

 
static inline bool __mei_wdt_is_registered(struct mei_wdt *wdt)
{
	return !!watchdog_get_drvdata(&wdt->wdd);
}

 
static void mei_wdt_unregister(struct mei_wdt *wdt)
{
	mutex_lock(&wdt->reg_lock);

	if (__mei_wdt_is_registered(wdt)) {
		watchdog_unregister_device(&wdt->wdd);
		watchdog_set_drvdata(&wdt->wdd, NULL);
		memset(&wdt->wdd, 0, sizeof(wdt->wdd));
	}

	mutex_unlock(&wdt->reg_lock);
}

 
static int mei_wdt_register(struct mei_wdt *wdt)
{
	struct device *dev;
	int ret;

	if (!wdt || !wdt->cldev)
		return -EINVAL;

	dev = &wdt->cldev->dev;

	mutex_lock(&wdt->reg_lock);

	if (__mei_wdt_is_registered(wdt)) {
		ret = 0;
		goto out;
	}

	wdt->wdd.info = &wd_info;
	wdt->wdd.ops = &wd_ops;
	wdt->wdd.parent = dev;
	wdt->wdd.timeout = MEI_WDT_DEFAULT_TIMEOUT;
	wdt->wdd.min_timeout = MEI_WDT_MIN_TIMEOUT;
	wdt->wdd.max_timeout = MEI_WDT_MAX_TIMEOUT;

	watchdog_set_drvdata(&wdt->wdd, wdt);
	watchdog_stop_on_reboot(&wdt->wdd);
	watchdog_stop_on_unregister(&wdt->wdd);

	ret = watchdog_register_device(&wdt->wdd);
	if (ret)
		watchdog_set_drvdata(&wdt->wdd, NULL);

	wdt->state = MEI_WDT_IDLE;

out:
	mutex_unlock(&wdt->reg_lock);
	return ret;
}

static void mei_wdt_unregister_work(struct work_struct *work)
{
	struct mei_wdt *wdt = container_of(work, struct mei_wdt, unregister);

	mei_wdt_unregister(wdt);
}

 
static void mei_wdt_rx(struct mei_cl_device *cldev)
{
	struct mei_wdt *wdt = mei_cldev_get_drvdata(cldev);
	struct mei_wdt_start_response res;
	const size_t res_len = sizeof(res);
	int ret;

	ret = mei_cldev_recv(wdt->cldev, (u8 *)&res, res_len);
	if (ret < 0) {
		dev_err(&cldev->dev, "failure in recv %d\n", ret);
		return;
	}

	 
	if (ret == 0)
		return;

	if (ret < sizeof(struct mei_mc_hdr)) {
		dev_err(&cldev->dev, "recv small data %d\n", ret);
		return;
	}

	if (res.hdr.command != MEI_MANAGEMENT_CONTROL ||
	    res.hdr.versionnumber != MEI_MC_VERSION_NUMBER) {
		dev_err(&cldev->dev, "wrong command received\n");
		return;
	}

	if (res.hdr.subcommand != MEI_MC_START_WD_TIMER_RES) {
		dev_warn(&cldev->dev, "unsupported command %d :%s[%d]\n",
			 res.hdr.subcommand,
			 mei_wdt_state_str(wdt->state),
			 wdt->state);
		return;
	}

	 
	if (wdt->state == MEI_WDT_RUNNING) {
		if (res.wdstate & MEI_WDT_WDSTATE_NOT_REQUIRED) {
			wdt->state = MEI_WDT_NOT_REQUIRED;
			schedule_work(&wdt->unregister);
		}
		goto out;
	}

	if (wdt->state == MEI_WDT_PROBE) {
		if (res.wdstate & MEI_WDT_WDSTATE_NOT_REQUIRED) {
			wdt->state = MEI_WDT_NOT_REQUIRED;
		} else {
			 
			mei_wdt_stop(wdt);
			mei_wdt_register(wdt);
		}
		return;
	}

	dev_warn(&cldev->dev, "not in correct state %s[%d]\n",
			 mei_wdt_state_str(wdt->state), wdt->state);

out:
	if (!completion_done(&wdt->response))
		complete(&wdt->response);
}

 
static void mei_wdt_notif(struct mei_cl_device *cldev)
{
	struct mei_wdt *wdt = mei_cldev_get_drvdata(cldev);

	if (wdt->state != MEI_WDT_NOT_REQUIRED)
		return;

	mei_wdt_register(wdt);
}

#if IS_ENABLED(CONFIG_DEBUG_FS)

static ssize_t mei_dbgfs_read_activation(struct file *file, char __user *ubuf,
					size_t cnt, loff_t *ppos)
{
	struct mei_wdt *wdt = file->private_data;
	const size_t bufsz = 32;
	char buf[32];
	ssize_t pos;

	mutex_lock(&wdt->reg_lock);
	pos = scnprintf(buf, bufsz, "%s\n",
		__mei_wdt_is_registered(wdt) ? "activated" : "deactivated");
	mutex_unlock(&wdt->reg_lock);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, pos);
}

static const struct file_operations dbgfs_fops_activation = {
	.open    = simple_open,
	.read    = mei_dbgfs_read_activation,
	.llseek  = generic_file_llseek,
};

static ssize_t mei_dbgfs_read_state(struct file *file, char __user *ubuf,
				    size_t cnt, loff_t *ppos)
{
	struct mei_wdt *wdt = file->private_data;
	char buf[32];
	ssize_t pos;

	pos = scnprintf(buf, sizeof(buf), "state: %s\n",
			mei_wdt_state_str(wdt->state));

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, pos);
}

static const struct file_operations dbgfs_fops_state = {
	.open = simple_open,
	.read = mei_dbgfs_read_state,
	.llseek = generic_file_llseek,
};

static void dbgfs_unregister(struct mei_wdt *wdt)
{
	debugfs_remove_recursive(wdt->dbgfs_dir);
	wdt->dbgfs_dir = NULL;
}

static void dbgfs_register(struct mei_wdt *wdt)
{
	struct dentry *dir;

	dir = debugfs_create_dir(KBUILD_MODNAME, NULL);
	wdt->dbgfs_dir = dir;

	debugfs_create_file("state", S_IRUSR, dir, wdt, &dbgfs_fops_state);

	debugfs_create_file("activation", S_IRUSR, dir, wdt,
			    &dbgfs_fops_activation);
}

#else

static inline void dbgfs_unregister(struct mei_wdt *wdt) {}
static inline void dbgfs_register(struct mei_wdt *wdt) {}
#endif  

static int mei_wdt_probe(struct mei_cl_device *cldev,
			 const struct mei_cl_device_id *id)
{
	struct mei_wdt *wdt;
	int ret;

	wdt = kzalloc(sizeof(struct mei_wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->timeout = MEI_WDT_DEFAULT_TIMEOUT;
	wdt->state = MEI_WDT_PROBE;
	wdt->cldev = cldev;
	wdt->resp_required = mei_cldev_ver(cldev) > 0x1;
	mutex_init(&wdt->reg_lock);
	init_completion(&wdt->response);
	INIT_WORK(&wdt->unregister, mei_wdt_unregister_work);

	mei_cldev_set_drvdata(cldev, wdt);

	ret = mei_cldev_enable(cldev);
	if (ret < 0) {
		dev_err(&cldev->dev, "Could not enable cl device\n");
		goto err_out;
	}

	ret = mei_cldev_register_rx_cb(wdt->cldev, mei_wdt_rx);
	if (ret) {
		dev_err(&cldev->dev, "Could not reg rx event ret=%d\n", ret);
		goto err_disable;
	}

	ret = mei_cldev_register_notif_cb(wdt->cldev, mei_wdt_notif);
	 
	if (ret && ret != -EOPNOTSUPP) {
		dev_err(&cldev->dev, "Could not reg notif event ret=%d\n", ret);
		goto err_disable;
	}

	wd_info.firmware_version = mei_cldev_ver(cldev);

	if (wdt->resp_required)
		ret = mei_wdt_ping(wdt);
	else
		ret = mei_wdt_register(wdt);

	if (ret)
		goto err_disable;

	dbgfs_register(wdt);

	return 0;

err_disable:
	mei_cldev_disable(cldev);

err_out:
	kfree(wdt);

	return ret;
}

static void mei_wdt_remove(struct mei_cl_device *cldev)
{
	struct mei_wdt *wdt = mei_cldev_get_drvdata(cldev);

	 
	if (!completion_done(&wdt->response))
		complete(&wdt->response);

	cancel_work_sync(&wdt->unregister);

	mei_wdt_unregister(wdt);

	mei_cldev_disable(cldev);

	dbgfs_unregister(wdt);

	kfree(wdt);
}

#define MEI_UUID_WD UUID_LE(0x05B79A6F, 0x4628, 0x4D7F, \
			    0x89, 0x9D, 0xA9, 0x15, 0x14, 0xCB, 0x32, 0xAB)

static const struct mei_cl_device_id mei_wdt_tbl[] = {
	{ .uuid = MEI_UUID_WD, .version = MEI_CL_VERSION_ANY },
	 
	{ }
};
MODULE_DEVICE_TABLE(mei, mei_wdt_tbl);

static struct mei_cl_driver mei_wdt_driver = {
	.id_table = mei_wdt_tbl,
	.name = KBUILD_MODNAME,

	.probe = mei_wdt_probe,
	.remove = mei_wdt_remove,
};

module_mei_cl_driver(mei_wdt_driver);

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Device driver for Intel MEI iAMT watchdog");
