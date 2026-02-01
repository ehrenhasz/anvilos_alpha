
 
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/sched/task.h>
#include <linux/sched/vhost_task.h>
#include <linux/sched/signal.h>

enum vhost_task_flags {
	VHOST_TASK_FLAGS_STOP,
};

struct vhost_task {
	bool (*fn)(void *data);
	void *data;
	struct completion exited;
	unsigned long flags;
	struct task_struct *task;
};

static int vhost_task_fn(void *data)
{
	struct vhost_task *vtsk = data;
	bool dead = false;

	for (;;) {
		bool did_work;

		if (!dead && signal_pending(current)) {
			struct ksignal ksig;
			 
			dead = get_signal(&ksig);
			if (dead)
				clear_thread_flag(TIF_SIGPENDING);
		}

		 
		set_current_state(TASK_INTERRUPTIBLE);

		if (test_bit(VHOST_TASK_FLAGS_STOP, &vtsk->flags)) {
			__set_current_state(TASK_RUNNING);
			break;
		}

		did_work = vtsk->fn(vtsk->data);
		if (!did_work)
			schedule();
	}

	complete(&vtsk->exited);
	do_exit(0);
}

 
void vhost_task_wake(struct vhost_task *vtsk)
{
	wake_up_process(vtsk->task);
}
EXPORT_SYMBOL_GPL(vhost_task_wake);

 
void vhost_task_stop(struct vhost_task *vtsk)
{
	set_bit(VHOST_TASK_FLAGS_STOP, &vtsk->flags);
	vhost_task_wake(vtsk);
	 
	wait_for_completion(&vtsk->exited);
	kfree(vtsk);
}
EXPORT_SYMBOL_GPL(vhost_task_stop);

 
struct vhost_task *vhost_task_create(bool (*fn)(void *), void *arg,
				     const char *name)
{
	struct kernel_clone_args args = {
		.flags		= CLONE_FS | CLONE_UNTRACED | CLONE_VM |
				  CLONE_THREAD | CLONE_SIGHAND,
		.exit_signal	= 0,
		.fn		= vhost_task_fn,
		.name		= name,
		.user_worker	= 1,
		.no_files	= 1,
	};
	struct vhost_task *vtsk;
	struct task_struct *tsk;

	vtsk = kzalloc(sizeof(*vtsk), GFP_KERNEL);
	if (!vtsk)
		return NULL;
	init_completion(&vtsk->exited);
	vtsk->data = arg;
	vtsk->fn = fn;

	args.fn_arg = vtsk;

	tsk = copy_process(NULL, 0, NUMA_NO_NODE, &args);
	if (IS_ERR(tsk)) {
		kfree(vtsk);
		return NULL;
	}

	vtsk->task = tsk;
	return vtsk;
}
EXPORT_SYMBOL_GPL(vhost_task_create);

 
void vhost_task_start(struct vhost_task *vtsk)
{
	wake_up_new_task(vtsk->task);
}
EXPORT_SYMBOL_GPL(vhost_task_start);
