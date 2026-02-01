

#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/list.h>
#include <linux/security.h>
#include <linux/umh.h>
#include <linux/sysctl.h>
#include <linux/module.h>

#include "fallback.h"
#include "firmware.h"

 

 
void fw_fallback_set_cache_timeout(void)
{
	fw_fallback_config.old_timeout = __firmware_loading_timeout();
	__fw_fallback_set_timeout(10);
}

 
void fw_fallback_set_default_timeout(void)
{
	__fw_fallback_set_timeout(fw_fallback_config.old_timeout);
}

static long firmware_loading_timeout(void)
{
	return __firmware_loading_timeout() > 0 ?
		__firmware_loading_timeout() * HZ : MAX_JIFFY_OFFSET;
}

static inline int fw_sysfs_wait_timeout(struct fw_priv *fw_priv,  long timeout)
{
	return __fw_state_wait_common(fw_priv, timeout);
}

static LIST_HEAD(pending_fw_head);

void kill_pending_fw_fallback_reqs(bool only_kill_custom)
{
	struct fw_priv *fw_priv;
	struct fw_priv *next;

	mutex_lock(&fw_lock);
	list_for_each_entry_safe(fw_priv, next, &pending_fw_head,
				 pending_list) {
		if (!fw_priv->need_uevent || !only_kill_custom)
			 __fw_load_abort(fw_priv);
	}
	mutex_unlock(&fw_lock);
}

 
static int fw_load_sysfs_fallback(struct fw_sysfs *fw_sysfs, long timeout)
{
	int retval = 0;
	struct device *f_dev = &fw_sysfs->dev;
	struct fw_priv *fw_priv = fw_sysfs->fw_priv;

	 
	if (!fw_priv->data)
		fw_priv->is_paged_buf = true;

	dev_set_uevent_suppress(f_dev, true);

	retval = device_add(f_dev);
	if (retval) {
		dev_err(f_dev, "%s: device_register failed\n", __func__);
		goto err_put_dev;
	}

	mutex_lock(&fw_lock);
	if (fw_state_is_aborted(fw_priv)) {
		mutex_unlock(&fw_lock);
		retval = -EINTR;
		goto out;
	}
	list_add(&fw_priv->pending_list, &pending_fw_head);
	mutex_unlock(&fw_lock);

	if (fw_priv->opt_flags & FW_OPT_UEVENT) {
		fw_priv->need_uevent = true;
		dev_set_uevent_suppress(f_dev, false);
		dev_dbg(f_dev, "firmware: requesting %s\n", fw_priv->fw_name);
		kobject_uevent(&fw_sysfs->dev.kobj, KOBJ_ADD);
	} else {
		timeout = MAX_JIFFY_OFFSET;
	}

	retval = fw_sysfs_wait_timeout(fw_priv, timeout);
	if (retval < 0 && retval != -ENOENT) {
		mutex_lock(&fw_lock);
		fw_load_abort(fw_sysfs);
		mutex_unlock(&fw_lock);
	}

	if (fw_state_is_aborted(fw_priv)) {
		if (retval == -ERESTARTSYS)
			retval = -EINTR;
	} else if (fw_priv->is_paged_buf && !fw_priv->data)
		retval = -ENOMEM;

out:
	device_del(f_dev);
err_put_dev:
	put_device(f_dev);
	return retval;
}

static int fw_load_from_user_helper(struct firmware *firmware,
				    const char *name, struct device *device,
				    u32 opt_flags)
{
	struct fw_sysfs *fw_sysfs;
	long timeout;
	int ret;

	timeout = firmware_loading_timeout();
	if (opt_flags & FW_OPT_NOWAIT) {
		timeout = usermodehelper_read_lock_wait(timeout);
		if (!timeout) {
			dev_dbg(device, "firmware: %s loading timed out\n",
				name);
			return -EBUSY;
		}
	} else {
		ret = usermodehelper_read_trylock();
		if (WARN_ON(ret)) {
			dev_err(device, "firmware: %s will not be loaded\n",
				name);
			return ret;
		}
	}

	fw_sysfs = fw_create_instance(firmware, name, device, opt_flags);
	if (IS_ERR(fw_sysfs)) {
		ret = PTR_ERR(fw_sysfs);
		goto out_unlock;
	}

	fw_sysfs->fw_priv = firmware->priv;
	ret = fw_load_sysfs_fallback(fw_sysfs, timeout);

	if (!ret)
		ret = assign_fw(firmware, device);

out_unlock:
	usermodehelper_read_unlock();

	return ret;
}

static bool fw_force_sysfs_fallback(u32 opt_flags)
{
	if (fw_fallback_config.force_sysfs_fallback)
		return true;
	if (!(opt_flags & FW_OPT_USERHELPER))
		return false;
	return true;
}

static bool fw_run_sysfs_fallback(u32 opt_flags)
{
	int ret;

	if (fw_fallback_config.ignore_sysfs_fallback) {
		pr_info_once("Ignoring firmware sysfs fallback due to sysctl knob\n");
		return false;
	}

	if ((opt_flags & FW_OPT_NOFALLBACK_SYSFS))
		return false;

	 
	ret = security_kernel_load_data(LOADING_FIRMWARE, true);
	if (ret < 0)
		return false;

	return fw_force_sysfs_fallback(opt_flags);
}

 
int firmware_fallback_sysfs(struct firmware *fw, const char *name,
			    struct device *device,
			    u32 opt_flags,
			    int ret)
{
	if (!fw_run_sysfs_fallback(opt_flags))
		return ret;

	if (!(opt_flags & FW_OPT_NO_WARN))
		dev_warn(device, "Falling back to sysfs fallback for: %s\n",
				 name);
	else
		dev_dbg(device, "Falling back to sysfs fallback for: %s\n",
				name);
	return fw_load_from_user_helper(fw, name, device, opt_flags);
}
