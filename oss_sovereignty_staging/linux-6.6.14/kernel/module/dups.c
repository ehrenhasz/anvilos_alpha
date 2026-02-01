 
 

#define pr_fmt(fmt)     "module: " fmt

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

#include "internal.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "module."
static bool enable_dups_trace = IS_ENABLED(CONFIG_MODULE_DEBUG_AUTOLOAD_DUPS_TRACE);
module_param(enable_dups_trace, bool_enable_only, 0644);

 
static DEFINE_MUTEX(kmod_dup_mutex);
static LIST_HEAD(dup_kmod_reqs);

struct kmod_dup_req {
	struct list_head list;
	char name[MODULE_NAME_LEN];
	struct completion first_req_done;
	struct work_struct complete_work;
	struct delayed_work delete_work;
	int dup_ret;
};

static struct kmod_dup_req *kmod_dup_request_lookup(char *module_name)
{
	struct kmod_dup_req *kmod_req;

	list_for_each_entry_rcu(kmod_req, &dup_kmod_reqs, list,
				lockdep_is_held(&kmod_dup_mutex)) {
		if (strlen(kmod_req->name) == strlen(module_name) &&
		    !memcmp(kmod_req->name, module_name, strlen(module_name))) {
			return kmod_req;
                }
        }

	return NULL;
}

static void kmod_dup_request_delete(struct work_struct *work)
{
	struct kmod_dup_req *kmod_req;
	kmod_req = container_of(to_delayed_work(work), struct kmod_dup_req, delete_work);

	 
	mutex_lock(&kmod_dup_mutex);
	list_del_rcu(&kmod_req->list);
	synchronize_rcu();
	mutex_unlock(&kmod_dup_mutex);
	kfree(kmod_req);
}

static void kmod_dup_request_complete(struct work_struct *work)
{
	struct kmod_dup_req *kmod_req;

	kmod_req = container_of(work, struct kmod_dup_req, complete_work);

	 
	complete_all(&kmod_req->first_req_done);

	 
	queue_delayed_work(system_wq, &kmod_req->delete_work, 60 * HZ);
}

bool kmod_dup_request_exists_wait(char *module_name, bool wait, int *dup_ret)
{
	struct kmod_dup_req *kmod_req, *new_kmod_req;
	int ret;

	 
	new_kmod_req = kzalloc(sizeof(*new_kmod_req), GFP_KERNEL);
	if (!new_kmod_req)
		return false;

	memcpy(new_kmod_req->name, module_name, strlen(module_name));
	INIT_WORK(&new_kmod_req->complete_work, kmod_dup_request_complete);
	INIT_DELAYED_WORK(&new_kmod_req->delete_work, kmod_dup_request_delete);
	init_completion(&new_kmod_req->first_req_done);

	mutex_lock(&kmod_dup_mutex);

	kmod_req = kmod_dup_request_lookup(module_name);
	if (!kmod_req) {
		 
		if (!wait) {
			kfree(new_kmod_req);
			pr_debug("New request_module_nowait() for %s -- cannot track duplicates for this request\n", module_name);
			mutex_unlock(&kmod_dup_mutex);
			return false;
		}

		 
		pr_debug("New request_module() for %s\n", module_name);
		list_add_rcu(&new_kmod_req->list, &dup_kmod_reqs);
		mutex_unlock(&kmod_dup_mutex);
		return false;
	}
	mutex_unlock(&kmod_dup_mutex);

	 
	kfree(new_kmod_req);

	 
	if (enable_dups_trace)
		WARN(1, "module-autoload: duplicate request for module %s\n", module_name);
	else
		pr_warn("module-autoload: duplicate request for module %s\n", module_name);

	if (!wait) {
		 
		*dup_ret = 0;
		return true;
	}

	 
	ret = wait_for_completion_state(&kmod_req->first_req_done,
					TASK_UNINTERRUPTIBLE | TASK_KILLABLE);
	if (ret) {
		*dup_ret = ret;
		return true;
	}

	 
	*dup_ret = kmod_req->dup_ret;

	return true;
}

void kmod_dup_request_announce(char *module_name, int ret)
{
	struct kmod_dup_req *kmod_req;

	mutex_lock(&kmod_dup_mutex);

	kmod_req = kmod_dup_request_lookup(module_name);
	if (!kmod_req)
		goto out;

	kmod_req->dup_ret = ret;

	 
	queue_work(system_wq, &kmod_req->complete_work);

out:
	mutex_unlock(&kmod_dup_mutex);
}
