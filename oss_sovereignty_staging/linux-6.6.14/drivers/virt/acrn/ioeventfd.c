
 

#include <linux/eventfd.h>
#include <linux/slab.h>

#include "acrn_drv.h"

 
struct hsm_ioeventfd {
	struct list_head	list;
	struct eventfd_ctx	*eventfd;
	u64			addr;
	u64			data;
	int			length;
	int			type;
	bool			wildcard;
};

static inline int ioreq_type_from_flags(int flags)
{
	return flags & ACRN_IOEVENTFD_FLAG_PIO ?
		       ACRN_IOREQ_TYPE_PORTIO : ACRN_IOREQ_TYPE_MMIO;
}

static void acrn_ioeventfd_shutdown(struct acrn_vm *vm, struct hsm_ioeventfd *p)
{
	lockdep_assert_held(&vm->ioeventfds_lock);

	eventfd_ctx_put(p->eventfd);
	list_del(&p->list);
	kfree(p);
}

static bool hsm_ioeventfd_is_conflict(struct acrn_vm *vm,
				      struct hsm_ioeventfd *ioeventfd)
{
	struct hsm_ioeventfd *p;

	lockdep_assert_held(&vm->ioeventfds_lock);

	 
	list_for_each_entry(p, &vm->ioeventfds, list)
		if (p->eventfd == ioeventfd->eventfd &&
		    p->addr == ioeventfd->addr &&
		    p->type == ioeventfd->type &&
		    (p->wildcard || ioeventfd->wildcard ||
			p->data == ioeventfd->data))
			return true;

	return false;
}

 
static int acrn_ioeventfd_assign(struct acrn_vm *vm,
				 struct acrn_ioeventfd *args)
{
	struct eventfd_ctx *eventfd;
	struct hsm_ioeventfd *p;
	int ret;

	 
	if (args->addr + args->len < args->addr)
		return -EINVAL;

	 
	if (!(args->len == 1 || args->len == 2 ||
	      args->len == 4 || args->len == 8))
		return -EINVAL;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p) {
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&p->list);
	p->addr = args->addr;
	p->length = args->len;
	p->eventfd = eventfd;
	p->type = ioreq_type_from_flags(args->flags);

	 
	if (args->flags & ACRN_IOEVENTFD_FLAG_DATAMATCH)
		p->data = args->data;
	else
		p->wildcard = true;

	mutex_lock(&vm->ioeventfds_lock);

	if (hsm_ioeventfd_is_conflict(vm, p)) {
		ret = -EEXIST;
		goto unlock_fail;
	}

	 
	ret = acrn_ioreq_range_add(vm->ioeventfd_client, p->type,
				   p->addr, p->addr + p->length - 1);
	if (ret < 0)
		goto unlock_fail;

	list_add_tail(&p->list, &vm->ioeventfds);
	mutex_unlock(&vm->ioeventfds_lock);

	return 0;

unlock_fail:
	mutex_unlock(&vm->ioeventfds_lock);
	kfree(p);
fail:
	eventfd_ctx_put(eventfd);
	return ret;
}

static int acrn_ioeventfd_deassign(struct acrn_vm *vm,
				   struct acrn_ioeventfd *args)
{
	struct hsm_ioeventfd *p;
	struct eventfd_ctx *eventfd;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	mutex_lock(&vm->ioeventfds_lock);
	list_for_each_entry(p, &vm->ioeventfds, list) {
		if (p->eventfd != eventfd)
			continue;

		acrn_ioreq_range_del(vm->ioeventfd_client, p->type,
				     p->addr, p->addr + p->length - 1);
		acrn_ioeventfd_shutdown(vm, p);
		break;
	}
	mutex_unlock(&vm->ioeventfds_lock);

	eventfd_ctx_put(eventfd);
	return 0;
}

static struct hsm_ioeventfd *hsm_ioeventfd_match(struct acrn_vm *vm, u64 addr,
						 u64 data, int len, int type)
{
	struct hsm_ioeventfd *p = NULL;

	lockdep_assert_held(&vm->ioeventfds_lock);

	list_for_each_entry(p, &vm->ioeventfds, list) {
		if (p->type == type && p->addr == addr && p->length >= len &&
		    (p->wildcard || p->data == data))
			return p;
	}

	return NULL;
}

static int acrn_ioeventfd_handler(struct acrn_ioreq_client *client,
				  struct acrn_io_request *req)
{
	struct hsm_ioeventfd *p;
	u64 addr, val;
	int size;

	if (req->type == ACRN_IOREQ_TYPE_MMIO) {
		 
		if (req->reqs.mmio_request.direction == ACRN_IOREQ_DIR_READ) {
			 
			req->reqs.mmio_request.value = 0;
			return 0;
		}
		addr = req->reqs.mmio_request.address;
		size = req->reqs.mmio_request.size;
		val = req->reqs.mmio_request.value;
	} else {
		if (req->reqs.pio_request.direction == ACRN_IOREQ_DIR_READ) {
			 
			req->reqs.pio_request.value = 0;
			return 0;
		}
		addr = req->reqs.pio_request.address;
		size = req->reqs.pio_request.size;
		val = req->reqs.pio_request.value;
	}

	mutex_lock(&client->vm->ioeventfds_lock);
	p = hsm_ioeventfd_match(client->vm, addr, val, size, req->type);
	if (p)
		eventfd_signal(p->eventfd, 1);
	mutex_unlock(&client->vm->ioeventfds_lock);

	return 0;
}

int acrn_ioeventfd_config(struct acrn_vm *vm, struct acrn_ioeventfd *args)
{
	int ret;

	if (args->flags & ACRN_IOEVENTFD_FLAG_DEASSIGN)
		ret = acrn_ioeventfd_deassign(vm, args);
	else
		ret = acrn_ioeventfd_assign(vm, args);

	return ret;
}

int acrn_ioeventfd_init(struct acrn_vm *vm)
{
	char name[ACRN_NAME_LEN];

	mutex_init(&vm->ioeventfds_lock);
	INIT_LIST_HEAD(&vm->ioeventfds);
	snprintf(name, sizeof(name), "ioeventfd-%u", vm->vmid);
	vm->ioeventfd_client = acrn_ioreq_client_create(vm,
							acrn_ioeventfd_handler,
							NULL, false, name);
	if (!vm->ioeventfd_client) {
		dev_err(acrn_dev.this_device, "Failed to create ioeventfd ioreq client!\n");
		return -EINVAL;
	}

	dev_dbg(acrn_dev.this_device, "VM %u ioeventfd init.\n", vm->vmid);
	return 0;
}

void acrn_ioeventfd_deinit(struct acrn_vm *vm)
{
	struct hsm_ioeventfd *p, *next;

	dev_dbg(acrn_dev.this_device, "VM %u ioeventfd deinit.\n", vm->vmid);
	acrn_ioreq_client_destroy(vm->ioeventfd_client);
	mutex_lock(&vm->ioeventfds_lock);
	list_for_each_entry_safe(p, next, &vm->ioeventfds, list)
		acrn_ioeventfd_shutdown(vm, p);
	mutex_unlock(&vm->ioeventfds_lock);
}
