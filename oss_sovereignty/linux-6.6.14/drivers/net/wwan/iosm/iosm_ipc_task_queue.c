
 

#include "iosm_ipc_imem.h"
#include "iosm_ipc_task_queue.h"

 
static void ipc_task_queue_handler(unsigned long data)
{
	struct ipc_task_queue *ipc_task = (struct ipc_task_queue *)data;
	unsigned int q_rpos = ipc_task->q_rpos;

	 
	while (q_rpos != ipc_task->q_wpos) {
		 
		struct ipc_task_queue_args *args = &ipc_task->args[q_rpos];

		 
		if (args->func)
			args->response = args->func(args->ipc_imem, args->arg,
						    args->msg, args->size);

		 
		if (args->completion)
			complete(args->completion);

		 
		if (args->is_copy)
			kfree(args->msg);

		 
		args->completion = NULL;
		args->func = NULL;
		args->msg = NULL;
		args->size = 0;
		args->is_copy = false;

		 
		q_rpos = (q_rpos + 1) % IPC_THREAD_QUEUE_SIZE;
		ipc_task->q_rpos = q_rpos;
	}
}

 
static void ipc_task_queue_cleanup(struct ipc_task_queue *ipc_task)
{
	unsigned int q_rpos = ipc_task->q_rpos;

	while (q_rpos != ipc_task->q_wpos) {
		struct ipc_task_queue_args *args = &ipc_task->args[q_rpos];

		if (args->completion)
			complete(args->completion);

		if (args->is_copy)
			kfree(args->msg);

		q_rpos = (q_rpos + 1) % IPC_THREAD_QUEUE_SIZE;
		ipc_task->q_rpos = q_rpos;
	}
}

 
static int
ipc_task_queue_add_task(struct iosm_imem *ipc_imem,
			int arg, void *msg,
			int (*func)(struct iosm_imem *ipc_imem, int arg,
				    void *msg, size_t size),
			size_t size, bool is_copy, bool wait)
{
	struct tasklet_struct *ipc_tasklet = ipc_imem->ipc_task->ipc_tasklet;
	struct ipc_task_queue *ipc_task = &ipc_imem->ipc_task->ipc_queue;
	struct completion completion;
	unsigned int pos, nextpos;
	unsigned long flags;
	int result = -EIO;

	init_completion(&completion);

	 
	spin_lock_irqsave(&ipc_task->q_lock, flags);

	pos = ipc_task->q_wpos;
	nextpos = (pos + 1) % IPC_THREAD_QUEUE_SIZE;

	 
	if (nextpos != ipc_task->q_rpos) {
		 
		ipc_task->args[pos].arg = arg;
		ipc_task->args[pos].msg = msg;
		ipc_task->args[pos].func = func;
		ipc_task->args[pos].ipc_imem = ipc_imem;
		ipc_task->args[pos].size = size;
		ipc_task->args[pos].is_copy = is_copy;
		ipc_task->args[pos].completion = wait ? &completion : NULL;
		ipc_task->args[pos].response = -1;

		 
		smp_wmb();

		 
		ipc_task->q_wpos = nextpos;
		result = 0;
	}

	spin_unlock_irqrestore(&ipc_task->q_lock, flags);

	if (result == 0) {
		tasklet_schedule(ipc_tasklet);

		if (wait) {
			wait_for_completion(&completion);
			result = ipc_task->args[pos].response;
		}
	} else {
		dev_err(ipc_imem->ipc_task->dev, "queue is full");
	}

	return result;
}

int ipc_task_queue_send_task(struct iosm_imem *imem,
			     int (*func)(struct iosm_imem *ipc_imem, int arg,
					 void *msg, size_t size),
			     int arg, void *msg, size_t size, bool wait)
{
	bool is_copy = false;
	void *copy = msg;
	int ret = -ENOMEM;

	if (size > 0) {
		copy = kmemdup(msg, size, GFP_ATOMIC);
		if (!copy)
			goto out;

		is_copy = true;
	}

	ret = ipc_task_queue_add_task(imem, arg, copy, func,
				      size, is_copy, wait);
	if (ret < 0) {
		dev_err(imem->ipc_task->dev,
			"add task failed for %ps %d, %p, %zu, %d", func, arg,
			copy, size, is_copy);
		if (is_copy)
			kfree(copy);
		goto out;
	}

	ret = 0;
out:
	return ret;
}

int ipc_task_init(struct ipc_task *ipc_task)
{
	struct ipc_task_queue *ipc_queue = &ipc_task->ipc_queue;

	ipc_task->ipc_tasklet = kzalloc(sizeof(*ipc_task->ipc_tasklet),
					GFP_KERNEL);

	if (!ipc_task->ipc_tasklet)
		return -ENOMEM;

	 
	spin_lock_init(&ipc_queue->q_lock);

	tasklet_init(ipc_task->ipc_tasklet, ipc_task_queue_handler,
		     (unsigned long)ipc_queue);
	return 0;
}

void ipc_task_deinit(struct ipc_task *ipc_task)
{
	tasklet_kill(ipc_task->ipc_tasklet);

	kfree(ipc_task->ipc_tasklet);
	 
	ipc_task_queue_cleanup(&ipc_task->ipc_queue);
}
