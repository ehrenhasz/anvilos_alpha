

 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/bitops.h>
#include <linux/btf.h>
#include <linux/btf_ids.h>
#include <linux/filter.h>
#include <linux/hid.h>
#include <linux/hid_bpf.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include "hid_bpf_dispatch.h"
#include "entrypoints/entrypoints.lskel.h"

struct hid_bpf_ops *hid_bpf_ops;
EXPORT_SYMBOL(hid_bpf_ops);

 
 
__weak noinline int hid_bpf_device_event(struct hid_bpf_ctx *ctx)
{
	return 0;
}

u8 *
dispatch_hid_bpf_device_event(struct hid_device *hdev, enum hid_report_type type, u8 *data,
			      u32 *size, int interrupt)
{
	struct hid_bpf_ctx_kern ctx_kern = {
		.ctx = {
			.hid = hdev,
			.report_type = type,
			.allocated_size = hdev->bpf.allocated_data,
			.size = *size,
		},
		.data = hdev->bpf.device_data,
	};
	int ret;

	if (type >= HID_REPORT_TYPES)
		return ERR_PTR(-EINVAL);

	 
	if (!hdev->bpf.device_data)
		return data;

	memset(ctx_kern.data, 0, hdev->bpf.allocated_data);
	memcpy(ctx_kern.data, data, *size);

	ret = hid_bpf_prog_run(hdev, HID_BPF_PROG_TYPE_DEVICE_EVENT, &ctx_kern);
	if (ret < 0)
		return ERR_PTR(ret);

	if (ret) {
		if (ret > ctx_kern.ctx.allocated_size)
			return ERR_PTR(-EINVAL);

		*size = ret;
	}

	return ctx_kern.data;
}
EXPORT_SYMBOL_GPL(dispatch_hid_bpf_device_event);

 
 
__weak noinline int hid_bpf_rdesc_fixup(struct hid_bpf_ctx *ctx)
{
	return 0;
}

u8 *call_hid_bpf_rdesc_fixup(struct hid_device *hdev, u8 *rdesc, unsigned int *size)
{
	int ret;
	struct hid_bpf_ctx_kern ctx_kern = {
		.ctx = {
			.hid = hdev,
			.size = *size,
			.allocated_size = HID_MAX_DESCRIPTOR_SIZE,
		},
	};

	ctx_kern.data = kzalloc(ctx_kern.ctx.allocated_size, GFP_KERNEL);
	if (!ctx_kern.data)
		goto ignore_bpf;

	memcpy(ctx_kern.data, rdesc, min_t(unsigned int, *size, HID_MAX_DESCRIPTOR_SIZE));

	ret = hid_bpf_prog_run(hdev, HID_BPF_PROG_TYPE_RDESC_FIXUP, &ctx_kern);
	if (ret < 0)
		goto ignore_bpf;

	if (ret) {
		if (ret > ctx_kern.ctx.allocated_size)
			goto ignore_bpf;

		*size = ret;
	}

	rdesc = krealloc(ctx_kern.data, *size, GFP_KERNEL);

	return rdesc;

 ignore_bpf:
	kfree(ctx_kern.data);
	return kmemdup(rdesc, *size, GFP_KERNEL);
}
EXPORT_SYMBOL_GPL(call_hid_bpf_rdesc_fixup);

 
noinline __u8 *
hid_bpf_get_data(struct hid_bpf_ctx *ctx, unsigned int offset, const size_t rdwr_buf_size)
{
	struct hid_bpf_ctx_kern *ctx_kern;

	if (!ctx)
		return NULL;

	ctx_kern = container_of(ctx, struct hid_bpf_ctx_kern, ctx);

	if (rdwr_buf_size + offset > ctx->allocated_size)
		return NULL;

	return ctx_kern->data + offset;
}

 
BTF_SET8_START(hid_bpf_kfunc_ids)
BTF_ID_FLAGS(func, hid_bpf_get_data, KF_RET_NULL)
BTF_SET8_END(hid_bpf_kfunc_ids)

static const struct btf_kfunc_id_set hid_bpf_kfunc_set = {
	.owner = THIS_MODULE,
	.set   = &hid_bpf_kfunc_ids,
};

static int device_match_id(struct device *dev, const void *id)
{
	struct hid_device *hdev = to_hid_device(dev);

	return hdev->id == *(int *)id;
}

static int __hid_bpf_allocate_data(struct hid_device *hdev, u8 **data, u32 *size)
{
	u8 *alloc_data;
	unsigned int i, j, max_report_len = 0;
	size_t alloc_size = 0;

	 
	for (i = 0; i < HID_REPORT_TYPES; i++) {
		struct hid_report_enum *report_enum = hdev->report_enum + i;

		for (j = 0; j < HID_MAX_IDS; j++) {
			struct hid_report *report = report_enum->report_id_hash[j];

			if (report)
				max_report_len = max(max_report_len, hid_report_len(report));
		}
	}

	 
	alloc_size = DIV_ROUND_UP(max_report_len, 64) * 64;

	alloc_data = kzalloc(alloc_size, GFP_KERNEL);
	if (!alloc_data)
		return -ENOMEM;

	*data = alloc_data;
	*size = alloc_size;

	return 0;
}

static int hid_bpf_allocate_event_data(struct hid_device *hdev)
{
	 
	if (hdev->bpf.device_data)
		return 0;

	return __hid_bpf_allocate_data(hdev, &hdev->bpf.device_data, &hdev->bpf.allocated_data);
}

int hid_bpf_reconnect(struct hid_device *hdev)
{
	if (!test_and_set_bit(ffs(HID_STAT_REPROBED), &hdev->status))
		return device_reprobe(&hdev->dev);

	return 0;
}

 
 
noinline int
hid_bpf_attach_prog(unsigned int hid_id, int prog_fd, __u32 flags)
{
	struct hid_device *hdev;
	struct device *dev;
	int fd, err, prog_type = hid_bpf_get_prog_attach_type(prog_fd);

	if (!hid_bpf_ops)
		return -EINVAL;

	if (prog_type < 0)
		return prog_type;

	if (prog_type >= HID_BPF_PROG_TYPE_MAX)
		return -EINVAL;

	if ((flags & ~HID_BPF_FLAG_MASK))
		return -EINVAL;

	dev = bus_find_device(hid_bpf_ops->bus_type, NULL, &hid_id, device_match_id);
	if (!dev)
		return -EINVAL;

	hdev = to_hid_device(dev);

	if (prog_type == HID_BPF_PROG_TYPE_DEVICE_EVENT) {
		err = hid_bpf_allocate_event_data(hdev);
		if (err)
			return err;
	}

	fd = __hid_bpf_attach_prog(hdev, prog_type, prog_fd, flags);
	if (fd < 0)
		return fd;

	if (prog_type == HID_BPF_PROG_TYPE_RDESC_FIXUP) {
		err = hid_bpf_reconnect(hdev);
		if (err) {
			close_fd(fd);
			return err;
		}
	}

	return fd;
}

 
noinline struct hid_bpf_ctx *
hid_bpf_allocate_context(unsigned int hid_id)
{
	struct hid_device *hdev;
	struct hid_bpf_ctx_kern *ctx_kern = NULL;
	struct device *dev;

	if (!hid_bpf_ops)
		return NULL;

	dev = bus_find_device(hid_bpf_ops->bus_type, NULL, &hid_id, device_match_id);
	if (!dev)
		return NULL;

	hdev = to_hid_device(dev);

	ctx_kern = kzalloc(sizeof(*ctx_kern), GFP_KERNEL);
	if (!ctx_kern)
		return NULL;

	ctx_kern->ctx.hid = hdev;

	return &ctx_kern->ctx;
}

 
noinline void
hid_bpf_release_context(struct hid_bpf_ctx *ctx)
{
	struct hid_bpf_ctx_kern *ctx_kern;

	ctx_kern = container_of(ctx, struct hid_bpf_ctx_kern, ctx);

	kfree(ctx_kern);
}

 
noinline int
hid_bpf_hw_request(struct hid_bpf_ctx *ctx, __u8 *buf, size_t buf__sz,
		   enum hid_report_type rtype, enum hid_class_request reqtype)
{
	struct hid_device *hdev;
	struct hid_report *report;
	struct hid_report_enum *report_enum;
	u8 *dma_data;
	u32 report_len;
	int ret;

	 
	if (!ctx || !hid_bpf_ops || !buf)
		return -EINVAL;

	switch (rtype) {
	case HID_INPUT_REPORT:
	case HID_OUTPUT_REPORT:
	case HID_FEATURE_REPORT:
		break;
	default:
		return -EINVAL;
	}

	switch (reqtype) {
	case HID_REQ_GET_REPORT:
	case HID_REQ_GET_IDLE:
	case HID_REQ_GET_PROTOCOL:
	case HID_REQ_SET_REPORT:
	case HID_REQ_SET_IDLE:
	case HID_REQ_SET_PROTOCOL:
		break;
	default:
		return -EINVAL;
	}

	if (buf__sz < 1)
		return -EINVAL;

	hdev = (struct hid_device *)ctx->hid;  

	report_enum = hdev->report_enum + rtype;
	report = hid_bpf_ops->hid_get_report(report_enum, buf);
	if (!report)
		return -EINVAL;

	report_len = hid_report_len(report);

	if (buf__sz > report_len)
		buf__sz = report_len;

	dma_data = kmemdup(buf, buf__sz, GFP_KERNEL);
	if (!dma_data)
		return -ENOMEM;

	ret = hid_bpf_ops->hid_hw_raw_request(hdev,
					      dma_data[0],
					      dma_data,
					      buf__sz,
					      rtype,
					      reqtype);

	if (ret > 0)
		memcpy(buf, dma_data, ret);

	kfree(dma_data);
	return ret;
}

 
BTF_SET8_START(hid_bpf_fmodret_ids)
BTF_ID_FLAGS(func, hid_bpf_device_event)
BTF_ID_FLAGS(func, hid_bpf_rdesc_fixup)
BTF_ID_FLAGS(func, __hid_bpf_tail_call)
BTF_SET8_END(hid_bpf_fmodret_ids)

static const struct btf_kfunc_id_set hid_bpf_fmodret_set = {
	.owner = THIS_MODULE,
	.set   = &hid_bpf_fmodret_ids,
};

 
BTF_SET8_START(hid_bpf_syscall_kfunc_ids)
BTF_ID_FLAGS(func, hid_bpf_attach_prog)
BTF_ID_FLAGS(func, hid_bpf_allocate_context, KF_ACQUIRE | KF_RET_NULL)
BTF_ID_FLAGS(func, hid_bpf_release_context, KF_RELEASE)
BTF_ID_FLAGS(func, hid_bpf_hw_request)
BTF_SET8_END(hid_bpf_syscall_kfunc_ids)

static const struct btf_kfunc_id_set hid_bpf_syscall_kfunc_set = {
	.owner = THIS_MODULE,
	.set   = &hid_bpf_syscall_kfunc_ids,
};

int hid_bpf_connect_device(struct hid_device *hdev)
{
	struct hid_bpf_prog_list *prog_list;

	rcu_read_lock();
	prog_list = rcu_dereference(hdev->bpf.progs[HID_BPF_PROG_TYPE_DEVICE_EVENT]);
	rcu_read_unlock();

	 
	if (!prog_list)
		return 0;

	return hid_bpf_allocate_event_data(hdev);
}
EXPORT_SYMBOL_GPL(hid_bpf_connect_device);

void hid_bpf_disconnect_device(struct hid_device *hdev)
{
	kfree(hdev->bpf.device_data);
	hdev->bpf.device_data = NULL;
	hdev->bpf.allocated_data = 0;
}
EXPORT_SYMBOL_GPL(hid_bpf_disconnect_device);

void hid_bpf_destroy_device(struct hid_device *hdev)
{
	if (!hdev)
		return;

	 
	hdev->bpf.destroyed = true;

	__hid_bpf_destroy_device(hdev);
}
EXPORT_SYMBOL_GPL(hid_bpf_destroy_device);

void hid_bpf_device_init(struct hid_device *hdev)
{
	spin_lock_init(&hdev->bpf.progs_lock);
}
EXPORT_SYMBOL_GPL(hid_bpf_device_init);

static int __init hid_bpf_init(void)
{
	int err;

	 

	err = register_btf_fmodret_id_set(&hid_bpf_fmodret_set);
	if (err) {
		pr_warn("error while registering fmodret entrypoints: %d", err);
		return 0;
	}

	err = hid_bpf_preload_skel();
	if (err) {
		pr_warn("error while preloading HID BPF dispatcher: %d", err);
		return 0;
	}

	 
	err = register_btf_kfunc_id_set(BPF_PROG_TYPE_TRACING, &hid_bpf_kfunc_set);
	if (err) {
		pr_warn("error while setting HID BPF tracing kfuncs: %d", err);
		return 0;
	}

	 
	err = register_btf_kfunc_id_set(BPF_PROG_TYPE_SYSCALL, &hid_bpf_syscall_kfunc_set);
	if (err) {
		pr_warn("error while setting HID BPF syscall kfuncs: %d", err);
		return 0;
	}

	return 0;
}

static void __exit hid_bpf_exit(void)
{
	 
	hid_bpf_free_links_and_skel();
}

late_initcall(hid_bpf_init);
module_exit(hid_bpf_exit);
MODULE_AUTHOR("Benjamin Tissoires");
MODULE_LICENSE("GPL");
