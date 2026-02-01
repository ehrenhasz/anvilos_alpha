 

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/binfmts.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/cred.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/workqueue.h>
#include <linux/security.h>
#include <linux/mount.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>
#include <linux/ptrace.h>
#include <linux/async.h>
#include <linux/uaccess.h>

#include <trace/events/module.h>
#include "internal.h"

 
#define MAX_KMOD_CONCURRENT 50
static DEFINE_SEMAPHORE(kmod_concurrent_max, MAX_KMOD_CONCURRENT);

 
#define MAX_KMOD_ALL_BUSY_TIMEOUT 5

 
char modprobe_path[KMOD_PATH_LEN] = CONFIG_MODPROBE_PATH;

static void free_modprobe_argv(struct subprocess_info *info)
{
	kfree(info->argv[3]);  
	kfree(info->argv);
}

static int call_modprobe(char *orig_module_name, int wait)
{
	struct subprocess_info *info;
	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
		NULL
	};
	char *module_name;
	int ret;

	char **argv = kmalloc(sizeof(char *[5]), GFP_KERNEL);
	if (!argv)
		goto out;

	module_name = kstrdup(orig_module_name, GFP_KERNEL);
	if (!module_name)
		goto free_argv;

	argv[0] = modprobe_path;
	argv[1] = "-q";
	argv[2] = "--";
	argv[3] = module_name;	 
	argv[4] = NULL;

	info = call_usermodehelper_setup(modprobe_path, argv, envp, GFP_KERNEL,
					 NULL, free_modprobe_argv, NULL);
	if (!info)
		goto free_module_name;

	ret = call_usermodehelper_exec(info, wait | UMH_KILLABLE);
	kmod_dup_request_announce(orig_module_name, ret);
	return ret;

free_module_name:
	kfree(module_name);
free_argv:
	kfree(argv);
out:
	kmod_dup_request_announce(orig_module_name, -ENOMEM);
	return -ENOMEM;
}

 
int __request_module(bool wait, const char *fmt, ...)
{
	va_list args;
	char module_name[MODULE_NAME_LEN];
	int ret, dup_ret;

	 
	WARN_ON_ONCE(wait && current_is_async());

	if (!modprobe_path[0])
		return -ENOENT;

	va_start(args, fmt);
	ret = vsnprintf(module_name, MODULE_NAME_LEN, fmt, args);
	va_end(args);
	if (ret >= MODULE_NAME_LEN)
		return -ENAMETOOLONG;

	ret = security_kernel_module_request(module_name);
	if (ret)
		return ret;

	ret = down_timeout(&kmod_concurrent_max, MAX_KMOD_ALL_BUSY_TIMEOUT * HZ);
	if (ret) {
		pr_warn_ratelimited("request_module: modprobe %s cannot be processed, kmod busy with %d threads for more than %d seconds now",
				    module_name, MAX_KMOD_CONCURRENT, MAX_KMOD_ALL_BUSY_TIMEOUT);
		return ret;
	}

	trace_module_request(module_name, wait, _RET_IP_);

	if (kmod_dup_request_exists_wait(module_name, wait, &dup_ret)) {
		ret = dup_ret;
		goto out;
	}

	ret = call_modprobe(module_name, wait ? UMH_WAIT_PROC : UMH_WAIT_EXEC);

out:
	up(&kmod_concurrent_max);

	return ret;
}
EXPORT_SYMBOL(__request_module);
