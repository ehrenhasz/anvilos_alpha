 

#ifndef IOSM_IPC_TASK_QUEUE_H
#define IOSM_IPC_TASK_QUEUE_H

 
#define IPC_THREAD_QUEUE_SIZE 256

 
struct ipc_task_queue_args {
	struct iosm_imem *ipc_imem;
	void *msg;
	struct completion *completion;
	int (*func)(struct iosm_imem *ipc_imem, int arg, void *msg,
		    size_t size);
	int arg;
	size_t size;
	int response;
	u8 is_copy:1;
};

 
struct ipc_task_queue {
	spinlock_t q_lock;  
	struct ipc_task_queue_args args[IPC_THREAD_QUEUE_SIZE];
	unsigned int q_rpos;
	unsigned int q_wpos;
};

 
struct ipc_task {
	struct device *dev;
	struct tasklet_struct *ipc_tasklet;
	struct ipc_task_queue ipc_queue;
};

 
int ipc_task_init(struct ipc_task *ipc_task);

 
void ipc_task_deinit(struct ipc_task *ipc_task);

 
int ipc_task_queue_send_task(struct iosm_imem *imem,
			     int (*func)(struct iosm_imem *ipc_imem, int arg,
					 void *msg, size_t size),
			     int arg, void *msg, size_t size, bool wait);

#endif
