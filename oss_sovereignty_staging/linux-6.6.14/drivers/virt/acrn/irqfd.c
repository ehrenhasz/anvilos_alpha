
 

#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>

#include "acrn_drv.h"

static LIST_HEAD(acrn_irqfd_clients);

 
struct hsm_irqfd {
	struct acrn_vm		*vm;
	wait_queue_entry_t	wait;
	struct work_struct	shutdown;
	struct eventfd_ctx	*eventfd;
	struct list_head	list;
	poll_table		pt;
	struct acrn_msi_entry	msi;
};

static void acrn_irqfd_inject(struct hsm_irqfd *irqfd)
{
	struct acrn_vm *vm = irqfd->vm;

	acrn_msi_inject(vm, irqfd->msi.msi_addr,
			irqfd->msi.msi_data);
}

static void hsm_irqfd_shutdown(struct hsm_irqfd *irqfd)
{
	u64 cnt;

	lockdep_assert_held(&irqfd->vm->irqfds_lock);

	 
	list_del_init(&irqfd->list);
	eventfd_ctx_remove_wait_queue(irqfd->eventfd, &irqfd->wait, &cnt);
	eventfd_ctx_put(irqfd->eventfd);
	kfree(irqfd);
}

static void hsm_irqfd_shutdown_work(struct work_struct *work)
{
	struct hsm_irqfd *irqfd;
	struct acrn_vm *vm;

	irqfd = container_of(work, struct hsm_irqfd, shutdown);
	vm = irqfd->vm;
	mutex_lock(&vm->irqfds_lock);
	if (!list_empty(&irqfd->list))
		hsm_irqfd_shutdown(irqfd);
	mutex_unlock(&vm->irqfds_lock);
}

 
static int hsm_irqfd_wakeup(wait_queue_entry_t *wait, unsigned int mode,
			    int sync, void *key)
{
	unsigned long poll_bits = (unsigned long)key;
	struct hsm_irqfd *irqfd;
	struct acrn_vm *vm;

	irqfd = container_of(wait, struct hsm_irqfd, wait);
	vm = irqfd->vm;
	if (poll_bits & POLLIN)
		 
		acrn_irqfd_inject(irqfd);

	if (poll_bits & POLLHUP)
		 
		queue_work(vm->irqfd_wq, &irqfd->shutdown);

	return 0;
}

static void hsm_irqfd_poll_func(struct file *file, wait_queue_head_t *wqh,
				poll_table *pt)
{
	struct hsm_irqfd *irqfd;

	irqfd = container_of(pt, struct hsm_irqfd, pt);
	add_wait_queue(wqh, &irqfd->wait);
}

 
static int acrn_irqfd_assign(struct acrn_vm *vm, struct acrn_irqfd *args)
{
	struct eventfd_ctx *eventfd = NULL;
	struct hsm_irqfd *irqfd, *tmp;
	__poll_t events;
	struct fd f;
	int ret = 0;

	irqfd = kzalloc(sizeof(*irqfd), GFP_KERNEL);
	if (!irqfd)
		return -ENOMEM;

	irqfd->vm = vm;
	memcpy(&irqfd->msi, &args->msi, sizeof(args->msi));
	INIT_LIST_HEAD(&irqfd->list);
	INIT_WORK(&irqfd->shutdown, hsm_irqfd_shutdown_work);

	f = fdget(args->fd);
	if (!f.file) {
		ret = -EBADF;
		goto out;
	}

	eventfd = eventfd_ctx_fileget(f.file);
	if (IS_ERR(eventfd)) {
		ret = PTR_ERR(eventfd);
		goto fail;
	}

	irqfd->eventfd = eventfd;

	 
	init_waitqueue_func_entry(&irqfd->wait, hsm_irqfd_wakeup);
	init_poll_funcptr(&irqfd->pt, hsm_irqfd_poll_func);

	mutex_lock(&vm->irqfds_lock);
	list_for_each_entry(tmp, &vm->irqfds, list) {
		if (irqfd->eventfd != tmp->eventfd)
			continue;
		ret = -EBUSY;
		mutex_unlock(&vm->irqfds_lock);
		goto fail;
	}
	list_add_tail(&irqfd->list, &vm->irqfds);
	mutex_unlock(&vm->irqfds_lock);

	 
	events = vfs_poll(f.file, &irqfd->pt);

	if (events & EPOLLIN)
		acrn_irqfd_inject(irqfd);

	fdput(f);
	return 0;
fail:
	if (eventfd && !IS_ERR(eventfd))
		eventfd_ctx_put(eventfd);

	fdput(f);
out:
	kfree(irqfd);
	return ret;
}

static int acrn_irqfd_deassign(struct acrn_vm *vm,
			       struct acrn_irqfd *args)
{
	struct hsm_irqfd *irqfd, *tmp;
	struct eventfd_ctx *eventfd;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	mutex_lock(&vm->irqfds_lock);
	list_for_each_entry_safe(irqfd, tmp, &vm->irqfds, list) {
		if (irqfd->eventfd == eventfd) {
			hsm_irqfd_shutdown(irqfd);
			break;
		}
	}
	mutex_unlock(&vm->irqfds_lock);
	eventfd_ctx_put(eventfd);

	return 0;
}

int acrn_irqfd_config(struct acrn_vm *vm, struct acrn_irqfd *args)
{
	int ret;

	if (args->flags & ACRN_IRQFD_FLAG_DEASSIGN)
		ret = acrn_irqfd_deassign(vm, args);
	else
		ret = acrn_irqfd_assign(vm, args);

	return ret;
}

int acrn_irqfd_init(struct acrn_vm *vm)
{
	INIT_LIST_HEAD(&vm->irqfds);
	mutex_init(&vm->irqfds_lock);
	vm->irqfd_wq = alloc_workqueue("acrn_irqfd-%u", 0, 0, vm->vmid);
	if (!vm->irqfd_wq)
		return -ENOMEM;

	dev_dbg(acrn_dev.this_device, "VM %u irqfd init.\n", vm->vmid);
	return 0;
}

void acrn_irqfd_deinit(struct acrn_vm *vm)
{
	struct hsm_irqfd *irqfd, *next;

	dev_dbg(acrn_dev.this_device, "VM %u irqfd deinit.\n", vm->vmid);
	destroy_workqueue(vm->irqfd_wq);
	mutex_lock(&vm->irqfds_lock);
	list_for_each_entry_safe(irqfd, next, &vm->irqfds, list)
		hsm_irqfd_shutdown(irqfd);
	mutex_unlock(&vm->irqfds_lock);
}
