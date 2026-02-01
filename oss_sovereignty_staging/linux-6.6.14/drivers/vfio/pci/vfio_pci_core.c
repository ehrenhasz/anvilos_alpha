
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/aperture.h>
#include <linux/device.h>
#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vgaarb.h>
#include <linux/nospec.h>
#include <linux/sched/mm.h>
#include <linux/iommufd.h>
#if IS_ENABLED(CONFIG_EEH)
#include <asm/eeh.h>
#endif

#include "vfio_pci_priv.h"

#define DRIVER_AUTHOR   "Alex Williamson <alex.williamson@redhat.com>"
#define DRIVER_DESC "core driver for VFIO based PCI devices"

static bool nointxmask;
static bool disable_vga;
static bool disable_idle_d3;

 
static DEFINE_MUTEX(vfio_pci_sriov_pfs_mutex);
static LIST_HEAD(vfio_pci_sriov_pfs);

struct vfio_pci_dummy_resource {
	struct resource		resource;
	int			index;
	struct list_head	res_next;
};

struct vfio_pci_vf_token {
	struct mutex		lock;
	uuid_t			uuid;
	int			users;
};

struct vfio_pci_mmap_vma {
	struct vm_area_struct	*vma;
	struct list_head	vma_next;
};

static inline bool vfio_vga_disabled(void)
{
#ifdef CONFIG_VFIO_PCI_VGA
	return disable_vga;
#else
	return true;
#endif
}

 
static unsigned int vfio_pci_set_decode(struct pci_dev *pdev, bool single_vga)
{
	struct pci_dev *tmp = NULL;
	unsigned char max_busnr;
	unsigned int decodes;

	if (single_vga || !vfio_vga_disabled() || pci_is_root_bus(pdev->bus))
		return VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM |
		       VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM;

	max_busnr = pci_bus_max_busnr(pdev->bus);
	decodes = VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM;

	while ((tmp = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, tmp)) != NULL) {
		if (tmp == pdev ||
		    pci_domain_nr(tmp->bus) != pci_domain_nr(pdev->bus) ||
		    pci_is_root_bus(tmp->bus))
			continue;

		if (tmp->bus->number >= pdev->bus->number &&
		    tmp->bus->number <= max_busnr) {
			pci_dev_put(tmp);
			decodes |= VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM;
			break;
		}
	}

	return decodes;
}

static void vfio_pci_probe_mmaps(struct vfio_pci_core_device *vdev)
{
	struct resource *res;
	int i;
	struct vfio_pci_dummy_resource *dummy_res;

	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		int bar = i + PCI_STD_RESOURCES;

		res = &vdev->pdev->resource[bar];

		if (!IS_ENABLED(CONFIG_VFIO_PCI_MMAP))
			goto no_mmap;

		if (!(res->flags & IORESOURCE_MEM))
			goto no_mmap;

		 
		if (!resource_size(res))
			goto no_mmap;

		if (resource_size(res) >= PAGE_SIZE) {
			vdev->bar_mmap_supported[bar] = true;
			continue;
		}

		if (!(res->start & ~PAGE_MASK)) {
			 
			dummy_res =
				kzalloc(sizeof(*dummy_res), GFP_KERNEL_ACCOUNT);
			if (dummy_res == NULL)
				goto no_mmap;

			dummy_res->resource.name = "vfio sub-page reserved";
			dummy_res->resource.start = res->end + 1;
			dummy_res->resource.end = res->start + PAGE_SIZE - 1;
			dummy_res->resource.flags = res->flags;
			if (request_resource(res->parent,
						&dummy_res->resource)) {
				kfree(dummy_res);
				goto no_mmap;
			}
			dummy_res->index = bar;
			list_add(&dummy_res->res_next,
					&vdev->dummy_resources_list);
			vdev->bar_mmap_supported[bar] = true;
			continue;
		}
		 
no_mmap:
		vdev->bar_mmap_supported[bar] = false;
	}
}

struct vfio_pci_group_info;
static void vfio_pci_dev_set_try_reset(struct vfio_device_set *dev_set);
static int vfio_pci_dev_set_hot_reset(struct vfio_device_set *dev_set,
				      struct vfio_pci_group_info *groups,
				      struct iommufd_ctx *iommufd_ctx);

 
static bool vfio_pci_nointx(struct pci_dev *pdev)
{
	switch (pdev->vendor) {
	case PCI_VENDOR_ID_INTEL:
		switch (pdev->device) {
		 
		case 0x1572:
		case 0x1574:
		case 0x1580 ... 0x1581:
		case 0x1583 ... 0x158b:
		case 0x37d0 ... 0x37d2:
		 
		case 0x1563:
			return true;
		default:
			return false;
		}
	}

	return false;
}

static void vfio_pci_probe_power_state(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;
	u16 pmcsr;

	if (!pdev->pm_cap)
		return;

	pci_read_config_word(pdev, pdev->pm_cap + PCI_PM_CTRL, &pmcsr);

	vdev->needs_pm_restore = !(pmcsr & PCI_PM_CTRL_NO_SOFT_RESET);
}

 
int vfio_pci_set_power_state(struct vfio_pci_core_device *vdev, pci_power_t state)
{
	struct pci_dev *pdev = vdev->pdev;
	bool needs_restore = false, needs_save = false;
	int ret;

	 
	if (pci_num_vf(pdev) && state > PCI_D0)
		return -EBUSY;

	if (vdev->needs_pm_restore) {
		if (pdev->current_state < PCI_D3hot && state >= PCI_D3hot) {
			pci_save_state(pdev);
			needs_save = true;
		}

		if (pdev->current_state >= PCI_D3hot && state <= PCI_D0)
			needs_restore = true;
	}

	ret = pci_set_power_state(pdev, state);

	if (!ret) {
		 
		if (needs_save && pdev->current_state >= PCI_D3hot) {
			 
			kfree(vdev->pm_save);
			vdev->pm_save = pci_store_saved_state(pdev);
		} else if (needs_restore) {
			pci_load_and_free_saved_state(pdev, &vdev->pm_save);
			pci_restore_state(pdev);
		}
	}

	return ret;
}

static int vfio_pci_runtime_pm_entry(struct vfio_pci_core_device *vdev,
				     struct eventfd_ctx *efdctx)
{
	 
	vfio_pci_zap_and_down_write_memory_lock(vdev);
	if (vdev->pm_runtime_engaged) {
		up_write(&vdev->memory_lock);
		return -EINVAL;
	}

	vdev->pm_runtime_engaged = true;
	vdev->pm_wake_eventfd_ctx = efdctx;
	pm_runtime_put_noidle(&vdev->pdev->dev);
	up_write(&vdev->memory_lock);

	return 0;
}

static int vfio_pci_core_pm_entry(struct vfio_device *device, u32 flags,
				  void __user *arg, size_t argsz)
{
	struct vfio_pci_core_device *vdev =
		container_of(device, struct vfio_pci_core_device, vdev);
	int ret;

	ret = vfio_check_feature(flags, argsz, VFIO_DEVICE_FEATURE_SET, 0);
	if (ret != 1)
		return ret;

	 
	return vfio_pci_runtime_pm_entry(vdev, NULL);
}

static int vfio_pci_core_pm_entry_with_wakeup(
	struct vfio_device *device, u32 flags,
	struct vfio_device_low_power_entry_with_wakeup __user *arg,
	size_t argsz)
{
	struct vfio_pci_core_device *vdev =
		container_of(device, struct vfio_pci_core_device, vdev);
	struct vfio_device_low_power_entry_with_wakeup entry;
	struct eventfd_ctx *efdctx;
	int ret;

	ret = vfio_check_feature(flags, argsz, VFIO_DEVICE_FEATURE_SET,
				 sizeof(entry));
	if (ret != 1)
		return ret;

	if (copy_from_user(&entry, arg, sizeof(entry)))
		return -EFAULT;

	if (entry.wakeup_eventfd < 0)
		return -EINVAL;

	efdctx = eventfd_ctx_fdget(entry.wakeup_eventfd);
	if (IS_ERR(efdctx))
		return PTR_ERR(efdctx);

	ret = vfio_pci_runtime_pm_entry(vdev, efdctx);
	if (ret)
		eventfd_ctx_put(efdctx);

	return ret;
}

static void __vfio_pci_runtime_pm_exit(struct vfio_pci_core_device *vdev)
{
	if (vdev->pm_runtime_engaged) {
		vdev->pm_runtime_engaged = false;
		pm_runtime_get_noresume(&vdev->pdev->dev);

		if (vdev->pm_wake_eventfd_ctx) {
			eventfd_ctx_put(vdev->pm_wake_eventfd_ctx);
			vdev->pm_wake_eventfd_ctx = NULL;
		}
	}
}

static void vfio_pci_runtime_pm_exit(struct vfio_pci_core_device *vdev)
{
	 
	down_write(&vdev->memory_lock);
	__vfio_pci_runtime_pm_exit(vdev);
	up_write(&vdev->memory_lock);
}

static int vfio_pci_core_pm_exit(struct vfio_device *device, u32 flags,
				 void __user *arg, size_t argsz)
{
	struct vfio_pci_core_device *vdev =
		container_of(device, struct vfio_pci_core_device, vdev);
	int ret;

	ret = vfio_check_feature(flags, argsz, VFIO_DEVICE_FEATURE_SET, 0);
	if (ret != 1)
		return ret;

	 
	vfio_pci_runtime_pm_exit(vdev);
	return 0;
}

#ifdef CONFIG_PM
static int vfio_pci_core_runtime_suspend(struct device *dev)
{
	struct vfio_pci_core_device *vdev = dev_get_drvdata(dev);

	down_write(&vdev->memory_lock);
	 
	vfio_pci_set_power_state(vdev, PCI_D0);
	up_write(&vdev->memory_lock);

	 
	vdev->pm_intx_masked = ((vdev->irq_type == VFIO_PCI_INTX_IRQ_INDEX) &&
				vfio_pci_intx_mask(vdev));

	return 0;
}

static int vfio_pci_core_runtime_resume(struct device *dev)
{
	struct vfio_pci_core_device *vdev = dev_get_drvdata(dev);

	 
	down_write(&vdev->memory_lock);
	if (vdev->pm_wake_eventfd_ctx) {
		eventfd_signal(vdev->pm_wake_eventfd_ctx, 1);
		__vfio_pci_runtime_pm_exit(vdev);
	}
	up_write(&vdev->memory_lock);

	if (vdev->pm_intx_masked)
		vfio_pci_intx_unmask(vdev);

	return 0;
}
#endif  

 
static const struct dev_pm_ops vfio_pci_core_pm_ops = {
	SET_RUNTIME_PM_OPS(vfio_pci_core_runtime_suspend,
			   vfio_pci_core_runtime_resume,
			   NULL)
};

int vfio_pci_core_enable(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;
	int ret;
	u16 cmd;
	u8 msix_pos;

	if (!disable_idle_d3) {
		ret = pm_runtime_resume_and_get(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	 
	pci_clear_master(pdev);

	ret = pci_enable_device(pdev);
	if (ret)
		goto out_power;

	 
	ret = pci_try_reset_function(pdev);
	if (ret == -EAGAIN)
		goto out_disable_device;

	vdev->reset_works = !ret;
	pci_save_state(pdev);
	vdev->pci_saved_state = pci_store_saved_state(pdev);
	if (!vdev->pci_saved_state)
		pci_dbg(pdev, "%s: Couldn't store saved state\n", __func__);

	if (likely(!nointxmask)) {
		if (vfio_pci_nointx(pdev)) {
			pci_info(pdev, "Masking broken INTx support\n");
			vdev->nointx = true;
			pci_intx(pdev, 0);
		} else
			vdev->pci_2_3 = pci_intx_mask_supported(pdev);
	}

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	if (vdev->pci_2_3 && (cmd & PCI_COMMAND_INTX_DISABLE)) {
		cmd &= ~PCI_COMMAND_INTX_DISABLE;
		pci_write_config_word(pdev, PCI_COMMAND, cmd);
	}

	ret = vfio_pci_zdev_open_device(vdev);
	if (ret)
		goto out_free_state;

	ret = vfio_config_init(vdev);
	if (ret)
		goto out_free_zdev;

	msix_pos = pdev->msix_cap;
	if (msix_pos) {
		u16 flags;
		u32 table;

		pci_read_config_word(pdev, msix_pos + PCI_MSIX_FLAGS, &flags);
		pci_read_config_dword(pdev, msix_pos + PCI_MSIX_TABLE, &table);

		vdev->msix_bar = table & PCI_MSIX_TABLE_BIR;
		vdev->msix_offset = table & PCI_MSIX_TABLE_OFFSET;
		vdev->msix_size = ((flags & PCI_MSIX_FLAGS_QSIZE) + 1) * 16;
		vdev->has_dyn_msix = pci_msix_can_alloc_dyn(pdev);
	} else {
		vdev->msix_bar = 0xFF;
		vdev->has_dyn_msix = false;
	}

	if (!vfio_vga_disabled() && vfio_pci_is_vga(pdev))
		vdev->has_vga = true;


	return 0;

out_free_zdev:
	vfio_pci_zdev_close_device(vdev);
out_free_state:
	kfree(vdev->pci_saved_state);
	vdev->pci_saved_state = NULL;
out_disable_device:
	pci_disable_device(pdev);
out_power:
	if (!disable_idle_d3)
		pm_runtime_put(&pdev->dev);
	return ret;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_enable);

void vfio_pci_core_disable(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;
	struct vfio_pci_dummy_resource *dummy_res, *tmp;
	struct vfio_pci_ioeventfd *ioeventfd, *ioeventfd_tmp;
	int i, bar;

	 
	lockdep_assert_held(&vdev->vdev.dev_set->lock);

	 
	vfio_pci_runtime_pm_exit(vdev);
	pm_runtime_resume(&pdev->dev);

	 
	vfio_pci_set_power_state(vdev, PCI_D0);

	 
	pci_clear_master(pdev);

	vfio_pci_set_irqs_ioctl(vdev, VFIO_IRQ_SET_DATA_NONE |
				VFIO_IRQ_SET_ACTION_TRIGGER,
				vdev->irq_type, 0, 0, NULL);

	 
	list_for_each_entry_safe(ioeventfd, ioeventfd_tmp,
				 &vdev->ioeventfds_list, next) {
		vfio_virqfd_disable(&ioeventfd->virqfd);
		list_del(&ioeventfd->next);
		kfree(ioeventfd);
	}
	vdev->ioeventfds_nr = 0;

	vdev->virq_disabled = false;

	for (i = 0; i < vdev->num_regions; i++)
		vdev->region[i].ops->release(vdev, &vdev->region[i]);

	vdev->num_regions = 0;
	kfree(vdev->region);
	vdev->region = NULL;  

	vfio_config_free(vdev);

	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		bar = i + PCI_STD_RESOURCES;
		if (!vdev->barmap[bar])
			continue;
		pci_iounmap(pdev, vdev->barmap[bar]);
		pci_release_selected_regions(pdev, 1 << bar);
		vdev->barmap[bar] = NULL;
	}

	list_for_each_entry_safe(dummy_res, tmp,
				 &vdev->dummy_resources_list, res_next) {
		list_del(&dummy_res->res_next);
		release_resource(&dummy_res->resource);
		kfree(dummy_res);
	}

	vdev->needs_reset = true;

	vfio_pci_zdev_close_device(vdev);

	 
	if (pci_load_and_free_saved_state(pdev, &vdev->pci_saved_state)) {
		pci_info(pdev, "%s: Couldn't reload saved state\n", __func__);

		if (!vdev->reset_works)
			goto out;

		pci_save_state(pdev);
	}

	 
	pci_write_config_word(pdev, PCI_COMMAND, PCI_COMMAND_INTX_DISABLE);

	 
	if (vdev->reset_works && pci_dev_trylock(pdev)) {
		if (!__pci_reset_function_locked(pdev))
			vdev->needs_reset = false;
		pci_dev_unlock(pdev);
	}

	pci_restore_state(pdev);
out:
	pci_disable_device(pdev);

	vfio_pci_dev_set_try_reset(vdev->vdev.dev_set);

	 
	if (!disable_idle_d3)
		pm_runtime_put(&pdev->dev);
}
EXPORT_SYMBOL_GPL(vfio_pci_core_disable);

void vfio_pci_core_close_device(struct vfio_device *core_vdev)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);

	if (vdev->sriov_pf_core_dev) {
		mutex_lock(&vdev->sriov_pf_core_dev->vf_token->lock);
		WARN_ON(!vdev->sriov_pf_core_dev->vf_token->users);
		vdev->sriov_pf_core_dev->vf_token->users--;
		mutex_unlock(&vdev->sriov_pf_core_dev->vf_token->lock);
	}
#if IS_ENABLED(CONFIG_EEH)
	eeh_dev_release(vdev->pdev);
#endif
	vfio_pci_core_disable(vdev);

	mutex_lock(&vdev->igate);
	if (vdev->err_trigger) {
		eventfd_ctx_put(vdev->err_trigger);
		vdev->err_trigger = NULL;
	}
	if (vdev->req_trigger) {
		eventfd_ctx_put(vdev->req_trigger);
		vdev->req_trigger = NULL;
	}
	mutex_unlock(&vdev->igate);
}
EXPORT_SYMBOL_GPL(vfio_pci_core_close_device);

void vfio_pci_core_finish_enable(struct vfio_pci_core_device *vdev)
{
	vfio_pci_probe_mmaps(vdev);
#if IS_ENABLED(CONFIG_EEH)
	eeh_dev_open(vdev->pdev);
#endif

	if (vdev->sriov_pf_core_dev) {
		mutex_lock(&vdev->sriov_pf_core_dev->vf_token->lock);
		vdev->sriov_pf_core_dev->vf_token->users++;
		mutex_unlock(&vdev->sriov_pf_core_dev->vf_token->lock);
	}
}
EXPORT_SYMBOL_GPL(vfio_pci_core_finish_enable);

static int vfio_pci_get_irq_count(struct vfio_pci_core_device *vdev, int irq_type)
{
	if (irq_type == VFIO_PCI_INTX_IRQ_INDEX) {
		u8 pin;

		if (!IS_ENABLED(CONFIG_VFIO_PCI_INTX) ||
		    vdev->nointx || vdev->pdev->is_virtfn)
			return 0;

		pci_read_config_byte(vdev->pdev, PCI_INTERRUPT_PIN, &pin);

		return pin ? 1 : 0;
	} else if (irq_type == VFIO_PCI_MSI_IRQ_INDEX) {
		u8 pos;
		u16 flags;

		pos = vdev->pdev->msi_cap;
		if (pos) {
			pci_read_config_word(vdev->pdev,
					     pos + PCI_MSI_FLAGS, &flags);
			return 1 << ((flags & PCI_MSI_FLAGS_QMASK) >> 1);
		}
	} else if (irq_type == VFIO_PCI_MSIX_IRQ_INDEX) {
		u8 pos;
		u16 flags;

		pos = vdev->pdev->msix_cap;
		if (pos) {
			pci_read_config_word(vdev->pdev,
					     pos + PCI_MSIX_FLAGS, &flags);

			return (flags & PCI_MSIX_FLAGS_QSIZE) + 1;
		}
	} else if (irq_type == VFIO_PCI_ERR_IRQ_INDEX) {
		if (pci_is_pcie(vdev->pdev))
			return 1;
	} else if (irq_type == VFIO_PCI_REQ_IRQ_INDEX) {
		return 1;
	}

	return 0;
}

static int vfio_pci_count_devs(struct pci_dev *pdev, void *data)
{
	(*(int *)data)++;
	return 0;
}

struct vfio_pci_fill_info {
	struct vfio_pci_dependent_device __user *devices;
	struct vfio_pci_dependent_device __user *devices_end;
	struct vfio_device *vdev;
	u32 count;
	u32 flags;
};

static int vfio_pci_fill_devs(struct pci_dev *pdev, void *data)
{
	struct vfio_pci_dependent_device info = {
		.segment = pci_domain_nr(pdev->bus),
		.bus = pdev->bus->number,
		.devfn = pdev->devfn,
	};
	struct vfio_pci_fill_info *fill = data;

	fill->count++;
	if (fill->devices >= fill->devices_end)
		return 0;

	if (fill->flags & VFIO_PCI_HOT_RESET_FLAG_DEV_ID) {
		struct iommufd_ctx *iommufd = vfio_iommufd_device_ictx(fill->vdev);
		struct vfio_device_set *dev_set = fill->vdev->dev_set;
		struct vfio_device *vdev;

		 
		vdev = vfio_find_device_in_devset(dev_set, &pdev->dev);
		if (!vdev) {
			info.devid = VFIO_PCI_DEVID_NOT_OWNED;
		} else {
			int id = vfio_iommufd_get_dev_id(vdev, iommufd);

			if (id > 0)
				info.devid = id;
			else if (id == -ENOENT)
				info.devid = VFIO_PCI_DEVID_OWNED;
			else
				info.devid = VFIO_PCI_DEVID_NOT_OWNED;
		}
		 
		if (info.devid == VFIO_PCI_DEVID_NOT_OWNED)
			fill->flags &= ~VFIO_PCI_HOT_RESET_FLAG_DEV_ID_OWNED;
	} else {
		struct iommu_group *iommu_group;

		iommu_group = iommu_group_get(&pdev->dev);
		if (!iommu_group)
			return -EPERM;  

		info.group_id = iommu_group_id(iommu_group);
		iommu_group_put(iommu_group);
	}

	if (copy_to_user(fill->devices, &info, sizeof(info)))
		return -EFAULT;
	fill->devices++;
	return 0;
}

struct vfio_pci_group_info {
	int count;
	struct file **files;
};

static bool vfio_pci_dev_below_slot(struct pci_dev *pdev, struct pci_slot *slot)
{
	for (; pdev; pdev = pdev->bus->self)
		if (pdev->bus == slot->bus)
			return (pdev->slot == slot);
	return false;
}

struct vfio_pci_walk_info {
	int (*fn)(struct pci_dev *pdev, void *data);
	void *data;
	struct pci_dev *pdev;
	bool slot;
	int ret;
};

static int vfio_pci_walk_wrapper(struct pci_dev *pdev, void *data)
{
	struct vfio_pci_walk_info *walk = data;

	if (!walk->slot || vfio_pci_dev_below_slot(pdev, walk->pdev->slot))
		walk->ret = walk->fn(pdev, walk->data);

	return walk->ret;
}

static int vfio_pci_for_each_slot_or_bus(struct pci_dev *pdev,
					 int (*fn)(struct pci_dev *,
						   void *data), void *data,
					 bool slot)
{
	struct vfio_pci_walk_info walk = {
		.fn = fn, .data = data, .pdev = pdev, .slot = slot, .ret = 0,
	};

	pci_walk_bus(pdev->bus, vfio_pci_walk_wrapper, &walk);

	return walk.ret;
}

static int msix_mmappable_cap(struct vfio_pci_core_device *vdev,
			      struct vfio_info_cap *caps)
{
	struct vfio_info_cap_header header = {
		.id = VFIO_REGION_INFO_CAP_MSIX_MAPPABLE,
		.version = 1
	};

	return vfio_info_add_capability(caps, &header, sizeof(header));
}

int vfio_pci_core_register_dev_region(struct vfio_pci_core_device *vdev,
				      unsigned int type, unsigned int subtype,
				      const struct vfio_pci_regops *ops,
				      size_t size, u32 flags, void *data)
{
	struct vfio_pci_region *region;

	region = krealloc(vdev->region,
			  (vdev->num_regions + 1) * sizeof(*region),
			  GFP_KERNEL_ACCOUNT);
	if (!region)
		return -ENOMEM;

	vdev->region = region;
	vdev->region[vdev->num_regions].type = type;
	vdev->region[vdev->num_regions].subtype = subtype;
	vdev->region[vdev->num_regions].ops = ops;
	vdev->region[vdev->num_regions].size = size;
	vdev->region[vdev->num_regions].flags = flags;
	vdev->region[vdev->num_regions].data = data;

	vdev->num_regions++;

	return 0;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_register_dev_region);

static int vfio_pci_info_atomic_cap(struct vfio_pci_core_device *vdev,
				    struct vfio_info_cap *caps)
{
	struct vfio_device_info_cap_pci_atomic_comp cap = {
		.header.id = VFIO_DEVICE_INFO_CAP_PCI_ATOMIC_COMP,
		.header.version = 1
	};
	struct pci_dev *pdev = pci_physfn(vdev->pdev);
	u32 devcap2;

	pcie_capability_read_dword(pdev, PCI_EXP_DEVCAP2, &devcap2);

	if ((devcap2 & PCI_EXP_DEVCAP2_ATOMIC_COMP32) &&
	    !pci_enable_atomic_ops_to_root(pdev, PCI_EXP_DEVCAP2_ATOMIC_COMP32))
		cap.flags |= VFIO_PCI_ATOMIC_COMP32;

	if ((devcap2 & PCI_EXP_DEVCAP2_ATOMIC_COMP64) &&
	    !pci_enable_atomic_ops_to_root(pdev, PCI_EXP_DEVCAP2_ATOMIC_COMP64))
		cap.flags |= VFIO_PCI_ATOMIC_COMP64;

	if ((devcap2 & PCI_EXP_DEVCAP2_ATOMIC_COMP128) &&
	    !pci_enable_atomic_ops_to_root(pdev,
					   PCI_EXP_DEVCAP2_ATOMIC_COMP128))
		cap.flags |= VFIO_PCI_ATOMIC_COMP128;

	if (!cap.flags)
		return -ENODEV;

	return vfio_info_add_capability(caps, &cap.header, sizeof(cap));
}

static int vfio_pci_ioctl_get_info(struct vfio_pci_core_device *vdev,
				   struct vfio_device_info __user *arg)
{
	unsigned long minsz = offsetofend(struct vfio_device_info, num_irqs);
	struct vfio_device_info info = {};
	struct vfio_info_cap caps = { .buf = NULL, .size = 0 };
	int ret;

	if (copy_from_user(&info, arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz)
		return -EINVAL;

	minsz = min_t(size_t, info.argsz, sizeof(info));

	info.flags = VFIO_DEVICE_FLAGS_PCI;

	if (vdev->reset_works)
		info.flags |= VFIO_DEVICE_FLAGS_RESET;

	info.num_regions = VFIO_PCI_NUM_REGIONS + vdev->num_regions;
	info.num_irqs = VFIO_PCI_NUM_IRQS;

	ret = vfio_pci_info_zdev_add_caps(vdev, &caps);
	if (ret && ret != -ENODEV) {
		pci_warn(vdev->pdev,
			 "Failed to setup zPCI info capabilities\n");
		return ret;
	}

	ret = vfio_pci_info_atomic_cap(vdev, &caps);
	if (ret && ret != -ENODEV) {
		pci_warn(vdev->pdev,
			 "Failed to setup AtomicOps info capability\n");
		return ret;
	}

	if (caps.size) {
		info.flags |= VFIO_DEVICE_FLAGS_CAPS;
		if (info.argsz < sizeof(info) + caps.size) {
			info.argsz = sizeof(info) + caps.size;
		} else {
			vfio_info_cap_shift(&caps, sizeof(info));
			if (copy_to_user(arg + 1, caps.buf, caps.size)) {
				kfree(caps.buf);
				return -EFAULT;
			}
			info.cap_offset = sizeof(*arg);
		}

		kfree(caps.buf);
	}

	return copy_to_user(arg, &info, minsz) ? -EFAULT : 0;
}

static int vfio_pci_ioctl_get_region_info(struct vfio_pci_core_device *vdev,
					  struct vfio_region_info __user *arg)
{
	unsigned long minsz = offsetofend(struct vfio_region_info, offset);
	struct pci_dev *pdev = vdev->pdev;
	struct vfio_region_info info;
	struct vfio_info_cap caps = { .buf = NULL, .size = 0 };
	int i, ret;

	if (copy_from_user(&info, arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz)
		return -EINVAL;

	switch (info.index) {
	case VFIO_PCI_CONFIG_REGION_INDEX:
		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.size = pdev->cfg_size;
		info.flags = VFIO_REGION_INFO_FLAG_READ |
			     VFIO_REGION_INFO_FLAG_WRITE;
		break;
	case VFIO_PCI_BAR0_REGION_INDEX ... VFIO_PCI_BAR5_REGION_INDEX:
		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.size = pci_resource_len(pdev, info.index);
		if (!info.size) {
			info.flags = 0;
			break;
		}

		info.flags = VFIO_REGION_INFO_FLAG_READ |
			     VFIO_REGION_INFO_FLAG_WRITE;
		if (vdev->bar_mmap_supported[info.index]) {
			info.flags |= VFIO_REGION_INFO_FLAG_MMAP;
			if (info.index == vdev->msix_bar) {
				ret = msix_mmappable_cap(vdev, &caps);
				if (ret)
					return ret;
			}
		}

		break;
	case VFIO_PCI_ROM_REGION_INDEX: {
		void __iomem *io;
		size_t size;
		u16 cmd;

		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.flags = 0;

		 
		info.size = pci_resource_len(pdev, info.index);
		if (!info.size) {
			 
			if (pdev->resource[PCI_ROM_RESOURCE].flags &
			    IORESOURCE_ROM_SHADOW)
				info.size = 0x20000;
			else
				break;
		}

		 
		cmd = vfio_pci_memory_lock_and_enable(vdev);
		io = pci_map_rom(pdev, &size);
		if (io) {
			info.flags = VFIO_REGION_INFO_FLAG_READ;
			pci_unmap_rom(pdev, io);
		} else {
			info.size = 0;
		}
		vfio_pci_memory_unlock_and_restore(vdev, cmd);

		break;
	}
	case VFIO_PCI_VGA_REGION_INDEX:
		if (!vdev->has_vga)
			return -EINVAL;

		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.size = 0xc0000;
		info.flags = VFIO_REGION_INFO_FLAG_READ |
			     VFIO_REGION_INFO_FLAG_WRITE;

		break;
	default: {
		struct vfio_region_info_cap_type cap_type = {
			.header.id = VFIO_REGION_INFO_CAP_TYPE,
			.header.version = 1
		};

		if (info.index >= VFIO_PCI_NUM_REGIONS + vdev->num_regions)
			return -EINVAL;
		info.index = array_index_nospec(
			info.index, VFIO_PCI_NUM_REGIONS + vdev->num_regions);

		i = info.index - VFIO_PCI_NUM_REGIONS;

		info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
		info.size = vdev->region[i].size;
		info.flags = vdev->region[i].flags;

		cap_type.type = vdev->region[i].type;
		cap_type.subtype = vdev->region[i].subtype;

		ret = vfio_info_add_capability(&caps, &cap_type.header,
					       sizeof(cap_type));
		if (ret)
			return ret;

		if (vdev->region[i].ops->add_capability) {
			ret = vdev->region[i].ops->add_capability(
				vdev, &vdev->region[i], &caps);
			if (ret)
				return ret;
		}
	}
	}

	if (caps.size) {
		info.flags |= VFIO_REGION_INFO_FLAG_CAPS;
		if (info.argsz < sizeof(info) + caps.size) {
			info.argsz = sizeof(info) + caps.size;
			info.cap_offset = 0;
		} else {
			vfio_info_cap_shift(&caps, sizeof(info));
			if (copy_to_user(arg + 1, caps.buf, caps.size)) {
				kfree(caps.buf);
				return -EFAULT;
			}
			info.cap_offset = sizeof(*arg);
		}

		kfree(caps.buf);
	}

	return copy_to_user(arg, &info, minsz) ? -EFAULT : 0;
}

static int vfio_pci_ioctl_get_irq_info(struct vfio_pci_core_device *vdev,
				       struct vfio_irq_info __user *arg)
{
	unsigned long minsz = offsetofend(struct vfio_irq_info, count);
	struct vfio_irq_info info;

	if (copy_from_user(&info, arg, minsz))
		return -EFAULT;

	if (info.argsz < minsz || info.index >= VFIO_PCI_NUM_IRQS)
		return -EINVAL;

	switch (info.index) {
	case VFIO_PCI_INTX_IRQ_INDEX ... VFIO_PCI_MSIX_IRQ_INDEX:
	case VFIO_PCI_REQ_IRQ_INDEX:
		break;
	case VFIO_PCI_ERR_IRQ_INDEX:
		if (pci_is_pcie(vdev->pdev))
			break;
		fallthrough;
	default:
		return -EINVAL;
	}

	info.flags = VFIO_IRQ_INFO_EVENTFD;

	info.count = vfio_pci_get_irq_count(vdev, info.index);

	if (info.index == VFIO_PCI_INTX_IRQ_INDEX)
		info.flags |=
			(VFIO_IRQ_INFO_MASKABLE | VFIO_IRQ_INFO_AUTOMASKED);
	else if (info.index != VFIO_PCI_MSIX_IRQ_INDEX || !vdev->has_dyn_msix)
		info.flags |= VFIO_IRQ_INFO_NORESIZE;

	return copy_to_user(arg, &info, minsz) ? -EFAULT : 0;
}

static int vfio_pci_ioctl_set_irqs(struct vfio_pci_core_device *vdev,
				   struct vfio_irq_set __user *arg)
{
	unsigned long minsz = offsetofend(struct vfio_irq_set, count);
	struct vfio_irq_set hdr;
	u8 *data = NULL;
	int max, ret = 0;
	size_t data_size = 0;

	if (copy_from_user(&hdr, arg, minsz))
		return -EFAULT;

	max = vfio_pci_get_irq_count(vdev, hdr.index);

	ret = vfio_set_irqs_validate_and_prepare(&hdr, max, VFIO_PCI_NUM_IRQS,
						 &data_size);
	if (ret)
		return ret;

	if (data_size) {
		data = memdup_user(&arg->data, data_size);
		if (IS_ERR(data))
			return PTR_ERR(data);
	}

	mutex_lock(&vdev->igate);

	ret = vfio_pci_set_irqs_ioctl(vdev, hdr.flags, hdr.index, hdr.start,
				      hdr.count, data);

	mutex_unlock(&vdev->igate);
	kfree(data);

	return ret;
}

static int vfio_pci_ioctl_reset(struct vfio_pci_core_device *vdev,
				void __user *arg)
{
	int ret;

	if (!vdev->reset_works)
		return -EINVAL;

	vfio_pci_zap_and_down_write_memory_lock(vdev);

	 
	vfio_pci_set_power_state(vdev, PCI_D0);

	ret = pci_try_reset_function(vdev->pdev);
	up_write(&vdev->memory_lock);

	return ret;
}

static int vfio_pci_ioctl_get_pci_hot_reset_info(
	struct vfio_pci_core_device *vdev,
	struct vfio_pci_hot_reset_info __user *arg)
{
	unsigned long minsz =
		offsetofend(struct vfio_pci_hot_reset_info, count);
	struct vfio_pci_hot_reset_info hdr;
	struct vfio_pci_fill_info fill = {};
	bool slot = false;
	int ret = 0;

	if (copy_from_user(&hdr, arg, minsz))
		return -EFAULT;

	if (hdr.argsz < minsz)
		return -EINVAL;

	hdr.flags = 0;

	 
	if (!pci_probe_reset_slot(vdev->pdev->slot))
		slot = true;
	else if (pci_probe_reset_bus(vdev->pdev->bus))
		return -ENODEV;

	fill.devices = arg->devices;
	fill.devices_end = arg->devices +
			   (hdr.argsz - sizeof(hdr)) / sizeof(arg->devices[0]);
	fill.vdev = &vdev->vdev;

	if (vfio_device_cdev_opened(&vdev->vdev))
		fill.flags |= VFIO_PCI_HOT_RESET_FLAG_DEV_ID |
			     VFIO_PCI_HOT_RESET_FLAG_DEV_ID_OWNED;

	mutex_lock(&vdev->vdev.dev_set->lock);
	ret = vfio_pci_for_each_slot_or_bus(vdev->pdev, vfio_pci_fill_devs,
					    &fill, slot);
	mutex_unlock(&vdev->vdev.dev_set->lock);
	if (ret)
		return ret;

	hdr.count = fill.count;
	hdr.flags = fill.flags;
	if (copy_to_user(arg, &hdr, minsz))
		return -EFAULT;

	if (fill.count > fill.devices - arg->devices)
		return -ENOSPC;
	return 0;
}

static int
vfio_pci_ioctl_pci_hot_reset_groups(struct vfio_pci_core_device *vdev,
				    int array_count, bool slot,
				    struct vfio_pci_hot_reset __user *arg)
{
	int32_t *group_fds;
	struct file **files;
	struct vfio_pci_group_info info;
	int file_idx, count = 0, ret = 0;

	 
	ret = vfio_pci_for_each_slot_or_bus(vdev->pdev, vfio_pci_count_devs,
					    &count, slot);
	if (ret)
		return ret;

	if (array_count > count)
		return -EINVAL;

	group_fds = kcalloc(array_count, sizeof(*group_fds), GFP_KERNEL);
	files = kcalloc(array_count, sizeof(*files), GFP_KERNEL);
	if (!group_fds || !files) {
		kfree(group_fds);
		kfree(files);
		return -ENOMEM;
	}

	if (copy_from_user(group_fds, arg->group_fds,
			   array_count * sizeof(*group_fds))) {
		kfree(group_fds);
		kfree(files);
		return -EFAULT;
	}

	 
	for (file_idx = 0; file_idx < array_count; file_idx++) {
		struct file *file = fget(group_fds[file_idx]);

		if (!file) {
			ret = -EBADF;
			break;
		}

		 
		if (!vfio_file_is_group(file)) {
			fput(file);
			ret = -EINVAL;
			break;
		}

		files[file_idx] = file;
	}

	kfree(group_fds);

	 
	if (ret)
		goto hot_reset_release;

	info.count = array_count;
	info.files = files;

	ret = vfio_pci_dev_set_hot_reset(vdev->vdev.dev_set, &info, NULL);

hot_reset_release:
	for (file_idx--; file_idx >= 0; file_idx--)
		fput(files[file_idx]);

	kfree(files);
	return ret;
}

static int vfio_pci_ioctl_pci_hot_reset(struct vfio_pci_core_device *vdev,
					struct vfio_pci_hot_reset __user *arg)
{
	unsigned long minsz = offsetofend(struct vfio_pci_hot_reset, count);
	struct vfio_pci_hot_reset hdr;
	bool slot = false;

	if (copy_from_user(&hdr, arg, minsz))
		return -EFAULT;

	if (hdr.argsz < minsz || hdr.flags)
		return -EINVAL;

	 
	if (!!hdr.count == vfio_device_cdev_opened(&vdev->vdev))
		return -EINVAL;

	 
	if (!pci_probe_reset_slot(vdev->pdev->slot))
		slot = true;
	else if (pci_probe_reset_bus(vdev->pdev->bus))
		return -ENODEV;

	if (hdr.count)
		return vfio_pci_ioctl_pci_hot_reset_groups(vdev, hdr.count, slot, arg);

	return vfio_pci_dev_set_hot_reset(vdev->vdev.dev_set, NULL,
					  vfio_iommufd_device_ictx(&vdev->vdev));
}

static int vfio_pci_ioctl_ioeventfd(struct vfio_pci_core_device *vdev,
				    struct vfio_device_ioeventfd __user *arg)
{
	unsigned long minsz = offsetofend(struct vfio_device_ioeventfd, fd);
	struct vfio_device_ioeventfd ioeventfd;
	int count;

	if (copy_from_user(&ioeventfd, arg, minsz))
		return -EFAULT;

	if (ioeventfd.argsz < minsz)
		return -EINVAL;

	if (ioeventfd.flags & ~VFIO_DEVICE_IOEVENTFD_SIZE_MASK)
		return -EINVAL;

	count = ioeventfd.flags & VFIO_DEVICE_IOEVENTFD_SIZE_MASK;

	if (hweight8(count) != 1 || ioeventfd.fd < -1)
		return -EINVAL;

	return vfio_pci_ioeventfd(vdev, ioeventfd.offset, ioeventfd.data, count,
				  ioeventfd.fd);
}

long vfio_pci_core_ioctl(struct vfio_device *core_vdev, unsigned int cmd,
			 unsigned long arg)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);
	void __user *uarg = (void __user *)arg;

	switch (cmd) {
	case VFIO_DEVICE_GET_INFO:
		return vfio_pci_ioctl_get_info(vdev, uarg);
	case VFIO_DEVICE_GET_IRQ_INFO:
		return vfio_pci_ioctl_get_irq_info(vdev, uarg);
	case VFIO_DEVICE_GET_PCI_HOT_RESET_INFO:
		return vfio_pci_ioctl_get_pci_hot_reset_info(vdev, uarg);
	case VFIO_DEVICE_GET_REGION_INFO:
		return vfio_pci_ioctl_get_region_info(vdev, uarg);
	case VFIO_DEVICE_IOEVENTFD:
		return vfio_pci_ioctl_ioeventfd(vdev, uarg);
	case VFIO_DEVICE_PCI_HOT_RESET:
		return vfio_pci_ioctl_pci_hot_reset(vdev, uarg);
	case VFIO_DEVICE_RESET:
		return vfio_pci_ioctl_reset(vdev, uarg);
	case VFIO_DEVICE_SET_IRQS:
		return vfio_pci_ioctl_set_irqs(vdev, uarg);
	default:
		return -ENOTTY;
	}
}
EXPORT_SYMBOL_GPL(vfio_pci_core_ioctl);

static int vfio_pci_core_feature_token(struct vfio_device *device, u32 flags,
				       uuid_t __user *arg, size_t argsz)
{
	struct vfio_pci_core_device *vdev =
		container_of(device, struct vfio_pci_core_device, vdev);
	uuid_t uuid;
	int ret;

	if (!vdev->vf_token)
		return -ENOTTY;
	 
	ret = vfio_check_feature(flags, argsz, VFIO_DEVICE_FEATURE_SET,
				 sizeof(uuid));
	if (ret != 1)
		return ret;

	if (copy_from_user(&uuid, arg, sizeof(uuid)))
		return -EFAULT;

	mutex_lock(&vdev->vf_token->lock);
	uuid_copy(&vdev->vf_token->uuid, &uuid);
	mutex_unlock(&vdev->vf_token->lock);
	return 0;
}

int vfio_pci_core_ioctl_feature(struct vfio_device *device, u32 flags,
				void __user *arg, size_t argsz)
{
	switch (flags & VFIO_DEVICE_FEATURE_MASK) {
	case VFIO_DEVICE_FEATURE_LOW_POWER_ENTRY:
		return vfio_pci_core_pm_entry(device, flags, arg, argsz);
	case VFIO_DEVICE_FEATURE_LOW_POWER_ENTRY_WITH_WAKEUP:
		return vfio_pci_core_pm_entry_with_wakeup(device, flags,
							  arg, argsz);
	case VFIO_DEVICE_FEATURE_LOW_POWER_EXIT:
		return vfio_pci_core_pm_exit(device, flags, arg, argsz);
	case VFIO_DEVICE_FEATURE_PCI_VF_TOKEN:
		return vfio_pci_core_feature_token(device, flags, arg, argsz);
	default:
		return -ENOTTY;
	}
}
EXPORT_SYMBOL_GPL(vfio_pci_core_ioctl_feature);

static ssize_t vfio_pci_rw(struct vfio_pci_core_device *vdev, char __user *buf,
			   size_t count, loff_t *ppos, bool iswrite)
{
	unsigned int index = VFIO_PCI_OFFSET_TO_INDEX(*ppos);
	int ret;

	if (index >= VFIO_PCI_NUM_REGIONS + vdev->num_regions)
		return -EINVAL;

	ret = pm_runtime_resume_and_get(&vdev->pdev->dev);
	if (ret) {
		pci_info_ratelimited(vdev->pdev, "runtime resume failed %d\n",
				     ret);
		return -EIO;
	}

	switch (index) {
	case VFIO_PCI_CONFIG_REGION_INDEX:
		ret = vfio_pci_config_rw(vdev, buf, count, ppos, iswrite);
		break;

	case VFIO_PCI_ROM_REGION_INDEX:
		if (iswrite)
			ret = -EINVAL;
		else
			ret = vfio_pci_bar_rw(vdev, buf, count, ppos, false);
		break;

	case VFIO_PCI_BAR0_REGION_INDEX ... VFIO_PCI_BAR5_REGION_INDEX:
		ret = vfio_pci_bar_rw(vdev, buf, count, ppos, iswrite);
		break;

	case VFIO_PCI_VGA_REGION_INDEX:
		ret = vfio_pci_vga_rw(vdev, buf, count, ppos, iswrite);
		break;

	default:
		index -= VFIO_PCI_NUM_REGIONS;
		ret = vdev->region[index].ops->rw(vdev, buf,
						   count, ppos, iswrite);
		break;
	}

	pm_runtime_put(&vdev->pdev->dev);
	return ret;
}

ssize_t vfio_pci_core_read(struct vfio_device *core_vdev, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);

	if (!count)
		return 0;

	return vfio_pci_rw(vdev, buf, count, ppos, false);
}
EXPORT_SYMBOL_GPL(vfio_pci_core_read);

ssize_t vfio_pci_core_write(struct vfio_device *core_vdev, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);

	if (!count)
		return 0;

	return vfio_pci_rw(vdev, (char __user *)buf, count, ppos, true);
}
EXPORT_SYMBOL_GPL(vfio_pci_core_write);

 
static int vfio_pci_zap_and_vma_lock(struct vfio_pci_core_device *vdev, bool try)
{
	struct vfio_pci_mmap_vma *mmap_vma, *tmp;

	 
	while (1) {
		struct mm_struct *mm = NULL;

		if (try) {
			if (!mutex_trylock(&vdev->vma_lock))
				return 0;
		} else {
			mutex_lock(&vdev->vma_lock);
		}
		while (!list_empty(&vdev->vma_list)) {
			mmap_vma = list_first_entry(&vdev->vma_list,
						    struct vfio_pci_mmap_vma,
						    vma_next);
			mm = mmap_vma->vma->vm_mm;
			if (mmget_not_zero(mm))
				break;

			list_del(&mmap_vma->vma_next);
			kfree(mmap_vma);
			mm = NULL;
		}
		if (!mm)
			return 1;
		mutex_unlock(&vdev->vma_lock);

		if (try) {
			if (!mmap_read_trylock(mm)) {
				mmput(mm);
				return 0;
			}
		} else {
			mmap_read_lock(mm);
		}
		if (try) {
			if (!mutex_trylock(&vdev->vma_lock)) {
				mmap_read_unlock(mm);
				mmput(mm);
				return 0;
			}
		} else {
			mutex_lock(&vdev->vma_lock);
		}
		list_for_each_entry_safe(mmap_vma, tmp,
					 &vdev->vma_list, vma_next) {
			struct vm_area_struct *vma = mmap_vma->vma;

			if (vma->vm_mm != mm)
				continue;

			list_del(&mmap_vma->vma_next);
			kfree(mmap_vma);

			zap_vma_ptes(vma, vma->vm_start,
				     vma->vm_end - vma->vm_start);
		}
		mutex_unlock(&vdev->vma_lock);
		mmap_read_unlock(mm);
		mmput(mm);
	}
}

void vfio_pci_zap_and_down_write_memory_lock(struct vfio_pci_core_device *vdev)
{
	vfio_pci_zap_and_vma_lock(vdev, false);
	down_write(&vdev->memory_lock);
	mutex_unlock(&vdev->vma_lock);
}

u16 vfio_pci_memory_lock_and_enable(struct vfio_pci_core_device *vdev)
{
	u16 cmd;

	down_write(&vdev->memory_lock);
	pci_read_config_word(vdev->pdev, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_MEMORY))
		pci_write_config_word(vdev->pdev, PCI_COMMAND,
				      cmd | PCI_COMMAND_MEMORY);

	return cmd;
}

void vfio_pci_memory_unlock_and_restore(struct vfio_pci_core_device *vdev, u16 cmd)
{
	pci_write_config_word(vdev->pdev, PCI_COMMAND, cmd);
	up_write(&vdev->memory_lock);
}

 
static int __vfio_pci_add_vma(struct vfio_pci_core_device *vdev,
			      struct vm_area_struct *vma)
{
	struct vfio_pci_mmap_vma *mmap_vma;

	mmap_vma = kmalloc(sizeof(*mmap_vma), GFP_KERNEL_ACCOUNT);
	if (!mmap_vma)
		return -ENOMEM;

	mmap_vma->vma = vma;
	list_add(&mmap_vma->vma_next, &vdev->vma_list);

	return 0;
}

 
static void vfio_pci_mmap_open(struct vm_area_struct *vma)
{
	zap_vma_ptes(vma, vma->vm_start, vma->vm_end - vma->vm_start);
}

static void vfio_pci_mmap_close(struct vm_area_struct *vma)
{
	struct vfio_pci_core_device *vdev = vma->vm_private_data;
	struct vfio_pci_mmap_vma *mmap_vma;

	mutex_lock(&vdev->vma_lock);
	list_for_each_entry(mmap_vma, &vdev->vma_list, vma_next) {
		if (mmap_vma->vma == vma) {
			list_del(&mmap_vma->vma_next);
			kfree(mmap_vma);
			break;
		}
	}
	mutex_unlock(&vdev->vma_lock);
}

static vm_fault_t vfio_pci_mmap_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct vfio_pci_core_device *vdev = vma->vm_private_data;
	struct vfio_pci_mmap_vma *mmap_vma;
	vm_fault_t ret = VM_FAULT_NOPAGE;

	mutex_lock(&vdev->vma_lock);
	down_read(&vdev->memory_lock);

	 
	if (vdev->pm_runtime_engaged || !__vfio_pci_memory_enabled(vdev)) {
		ret = VM_FAULT_SIGBUS;
		goto up_out;
	}

	 
	list_for_each_entry(mmap_vma, &vdev->vma_list, vma_next) {
		if (mmap_vma->vma == vma)
			goto up_out;
	}

	if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot)) {
		ret = VM_FAULT_SIGBUS;
		zap_vma_ptes(vma, vma->vm_start, vma->vm_end - vma->vm_start);
		goto up_out;
	}

	if (__vfio_pci_add_vma(vdev, vma)) {
		ret = VM_FAULT_OOM;
		zap_vma_ptes(vma, vma->vm_start, vma->vm_end - vma->vm_start);
	}

up_out:
	up_read(&vdev->memory_lock);
	mutex_unlock(&vdev->vma_lock);
	return ret;
}

static const struct vm_operations_struct vfio_pci_mmap_ops = {
	.open = vfio_pci_mmap_open,
	.close = vfio_pci_mmap_close,
	.fault = vfio_pci_mmap_fault,
};

int vfio_pci_core_mmap(struct vfio_device *core_vdev, struct vm_area_struct *vma)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);
	struct pci_dev *pdev = vdev->pdev;
	unsigned int index;
	u64 phys_len, req_len, pgoff, req_start;
	int ret;

	index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT);

	if (index >= VFIO_PCI_NUM_REGIONS + vdev->num_regions)
		return -EINVAL;
	if (vma->vm_end < vma->vm_start)
		return -EINVAL;
	if ((vma->vm_flags & VM_SHARED) == 0)
		return -EINVAL;
	if (index >= VFIO_PCI_NUM_REGIONS) {
		int regnum = index - VFIO_PCI_NUM_REGIONS;
		struct vfio_pci_region *region = vdev->region + regnum;

		if (region->ops && region->ops->mmap &&
		    (region->flags & VFIO_REGION_INFO_FLAG_MMAP))
			return region->ops->mmap(vdev, region, vma);
		return -EINVAL;
	}
	if (index >= VFIO_PCI_ROM_REGION_INDEX)
		return -EINVAL;
	if (!vdev->bar_mmap_supported[index])
		return -EINVAL;

	phys_len = PAGE_ALIGN(pci_resource_len(pdev, index));
	req_len = vma->vm_end - vma->vm_start;
	pgoff = vma->vm_pgoff &
		((1U << (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT)) - 1);
	req_start = pgoff << PAGE_SHIFT;

	if (req_start + req_len > phys_len)
		return -EINVAL;

	 
	if (!vdev->barmap[index]) {
		ret = pci_request_selected_regions(pdev,
						   1 << index, "vfio-pci");
		if (ret)
			return ret;

		vdev->barmap[index] = pci_iomap(pdev, index, 0);
		if (!vdev->barmap[index]) {
			pci_release_selected_regions(pdev, 1 << index);
			return -ENOMEM;
		}
	}

	vma->vm_private_data = vdev;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff = (pci_resource_start(pdev, index) >> PAGE_SHIFT) + pgoff;

	 
	vm_flags_set(vma, VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_ops = &vfio_pci_mmap_ops;

	return 0;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_mmap);

void vfio_pci_core_request(struct vfio_device *core_vdev, unsigned int count)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);
	struct pci_dev *pdev = vdev->pdev;

	mutex_lock(&vdev->igate);

	if (vdev->req_trigger) {
		if (!(count % 10))
			pci_notice_ratelimited(pdev,
				"Relaying device request to user (#%u)\n",
				count);
		eventfd_signal(vdev->req_trigger, 1);
	} else if (count == 0) {
		pci_warn(pdev,
			"No device request channel registered, blocked until released by user\n");
	}

	mutex_unlock(&vdev->igate);
}
EXPORT_SYMBOL_GPL(vfio_pci_core_request);

static int vfio_pci_validate_vf_token(struct vfio_pci_core_device *vdev,
				      bool vf_token, uuid_t *uuid)
{
	 
	if (vdev->pdev->is_virtfn) {
		struct vfio_pci_core_device *pf_vdev = vdev->sriov_pf_core_dev;
		bool match;

		if (!pf_vdev) {
			if (!vf_token)
				return 0;  

			pci_info_ratelimited(vdev->pdev,
				"VF token incorrectly provided, PF not bound to vfio-pci\n");
			return -EINVAL;
		}

		if (!vf_token) {
			pci_info_ratelimited(vdev->pdev,
				"VF token required to access device\n");
			return -EACCES;
		}

		mutex_lock(&pf_vdev->vf_token->lock);
		match = uuid_equal(uuid, &pf_vdev->vf_token->uuid);
		mutex_unlock(&pf_vdev->vf_token->lock);

		if (!match) {
			pci_info_ratelimited(vdev->pdev,
				"Incorrect VF token provided for device\n");
			return -EACCES;
		}
	} else if (vdev->vf_token) {
		mutex_lock(&vdev->vf_token->lock);
		if (vdev->vf_token->users) {
			if (!vf_token) {
				mutex_unlock(&vdev->vf_token->lock);
				pci_info_ratelimited(vdev->pdev,
					"VF token required to access device\n");
				return -EACCES;
			}

			if (!uuid_equal(uuid, &vdev->vf_token->uuid)) {
				mutex_unlock(&vdev->vf_token->lock);
				pci_info_ratelimited(vdev->pdev,
					"Incorrect VF token provided for device\n");
				return -EACCES;
			}
		} else if (vf_token) {
			uuid_copy(&vdev->vf_token->uuid, uuid);
		}

		mutex_unlock(&vdev->vf_token->lock);
	} else if (vf_token) {
		pci_info_ratelimited(vdev->pdev,
			"VF token incorrectly provided, not a PF or VF\n");
		return -EINVAL;
	}

	return 0;
}

#define VF_TOKEN_ARG "vf_token="

int vfio_pci_core_match(struct vfio_device *core_vdev, char *buf)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);
	bool vf_token = false;
	uuid_t uuid;
	int ret;

	if (strncmp(pci_name(vdev->pdev), buf, strlen(pci_name(vdev->pdev))))
		return 0;  

	if (strlen(buf) > strlen(pci_name(vdev->pdev))) {
		buf += strlen(pci_name(vdev->pdev));

		if (*buf != ' ')
			return 0;  

		while (*buf) {
			if (*buf == ' ') {
				buf++;
				continue;
			}

			if (!vf_token && !strncmp(buf, VF_TOKEN_ARG,
						  strlen(VF_TOKEN_ARG))) {
				buf += strlen(VF_TOKEN_ARG);

				if (strlen(buf) < UUID_STRING_LEN)
					return -EINVAL;

				ret = uuid_parse(buf, &uuid);
				if (ret)
					return ret;

				vf_token = true;
				buf += UUID_STRING_LEN;
			} else {
				 
				return -EINVAL;
			}
		}
	}

	ret = vfio_pci_validate_vf_token(vdev, vf_token, &uuid);
	if (ret)
		return ret;

	return 1;  
}
EXPORT_SYMBOL_GPL(vfio_pci_core_match);

static int vfio_pci_bus_notifier(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct vfio_pci_core_device *vdev = container_of(nb,
						    struct vfio_pci_core_device, nb);
	struct device *dev = data;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_dev *physfn = pci_physfn(pdev);

	if (action == BUS_NOTIFY_ADD_DEVICE &&
	    pdev->is_virtfn && physfn == vdev->pdev) {
		pci_info(vdev->pdev, "Captured SR-IOV VF %s driver_override\n",
			 pci_name(pdev));
		pdev->driver_override = kasprintf(GFP_KERNEL, "%s",
						  vdev->vdev.ops->name);
	} else if (action == BUS_NOTIFY_BOUND_DRIVER &&
		   pdev->is_virtfn && physfn == vdev->pdev) {
		struct pci_driver *drv = pci_dev_driver(pdev);

		if (drv && drv != pci_dev_driver(vdev->pdev))
			pci_warn(vdev->pdev,
				 "VF %s bound to driver %s while PF bound to driver %s\n",
				 pci_name(pdev), drv->name,
				 pci_dev_driver(vdev->pdev)->name);
	}

	return 0;
}

static int vfio_pci_vf_init(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;
	struct vfio_pci_core_device *cur;
	struct pci_dev *physfn;
	int ret;

	if (pdev->is_virtfn) {
		 
		physfn = pci_physfn(vdev->pdev);
		mutex_lock(&vfio_pci_sriov_pfs_mutex);
		list_for_each_entry(cur, &vfio_pci_sriov_pfs, sriov_pfs_item) {
			if (cur->pdev == physfn) {
				vdev->sriov_pf_core_dev = cur;
				break;
			}
		}
		mutex_unlock(&vfio_pci_sriov_pfs_mutex);
		return 0;
	}

	 
	if (!pdev->is_physfn)
		return 0;

	vdev->vf_token = kzalloc(sizeof(*vdev->vf_token), GFP_KERNEL);
	if (!vdev->vf_token)
		return -ENOMEM;

	mutex_init(&vdev->vf_token->lock);
	uuid_gen(&vdev->vf_token->uuid);

	vdev->nb.notifier_call = vfio_pci_bus_notifier;
	ret = bus_register_notifier(&pci_bus_type, &vdev->nb);
	if (ret) {
		kfree(vdev->vf_token);
		return ret;
	}
	return 0;
}

static void vfio_pci_vf_uninit(struct vfio_pci_core_device *vdev)
{
	if (!vdev->vf_token)
		return;

	bus_unregister_notifier(&pci_bus_type, &vdev->nb);
	WARN_ON(vdev->vf_token->users);
	mutex_destroy(&vdev->vf_token->lock);
	kfree(vdev->vf_token);
}

static int vfio_pci_vga_init(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;
	int ret;

	if (!vfio_pci_is_vga(pdev))
		return 0;

	ret = aperture_remove_conflicting_pci_devices(pdev, vdev->vdev.ops->name);
	if (ret)
		return ret;

	ret = vga_client_register(pdev, vfio_pci_set_decode);
	if (ret)
		return ret;
	vga_set_legacy_decoding(pdev, vfio_pci_set_decode(pdev, false));
	return 0;
}

static void vfio_pci_vga_uninit(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;

	if (!vfio_pci_is_vga(pdev))
		return;
	vga_client_unregister(pdev);
	vga_set_legacy_decoding(pdev, VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM |
					      VGA_RSRC_LEGACY_IO |
					      VGA_RSRC_LEGACY_MEM);
}

int vfio_pci_core_init_dev(struct vfio_device *core_vdev)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);

	vdev->pdev = to_pci_dev(core_vdev->dev);
	vdev->irq_type = VFIO_PCI_NUM_IRQS;
	mutex_init(&vdev->igate);
	spin_lock_init(&vdev->irqlock);
	mutex_init(&vdev->ioeventfds_lock);
	INIT_LIST_HEAD(&vdev->dummy_resources_list);
	INIT_LIST_HEAD(&vdev->ioeventfds_list);
	mutex_init(&vdev->vma_lock);
	INIT_LIST_HEAD(&vdev->vma_list);
	INIT_LIST_HEAD(&vdev->sriov_pfs_item);
	init_rwsem(&vdev->memory_lock);
	xa_init(&vdev->ctx);

	return 0;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_init_dev);

void vfio_pci_core_release_dev(struct vfio_device *core_vdev)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);

	mutex_destroy(&vdev->igate);
	mutex_destroy(&vdev->ioeventfds_lock);
	mutex_destroy(&vdev->vma_lock);
	kfree(vdev->region);
	kfree(vdev->pm_save);
}
EXPORT_SYMBOL_GPL(vfio_pci_core_release_dev);

int vfio_pci_core_register_device(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *pdev = vdev->pdev;
	struct device *dev = &pdev->dev;
	int ret;

	 
	if (WARN_ON(vdev != dev_get_drvdata(dev)))
		return -EINVAL;

	if (pdev->hdr_type != PCI_HEADER_TYPE_NORMAL)
		return -EINVAL;

	if (vdev->vdev.mig_ops) {
		if (!(vdev->vdev.mig_ops->migration_get_state &&
		      vdev->vdev.mig_ops->migration_set_state &&
		      vdev->vdev.mig_ops->migration_get_data_size) ||
		    !(vdev->vdev.migration_flags & VFIO_MIGRATION_STOP_COPY))
			return -EINVAL;
	}

	if (vdev->vdev.log_ops && !(vdev->vdev.log_ops->log_start &&
	    vdev->vdev.log_ops->log_stop &&
	    vdev->vdev.log_ops->log_read_and_clear))
		return -EINVAL;

	 
	if (pci_num_vf(pdev)) {
		pci_warn(pdev, "Cannot bind to PF with SR-IOV enabled\n");
		return -EBUSY;
	}

	if (pci_is_root_bus(pdev->bus)) {
		ret = vfio_assign_device_set(&vdev->vdev, vdev);
	} else if (!pci_probe_reset_slot(pdev->slot)) {
		ret = vfio_assign_device_set(&vdev->vdev, pdev->slot);
	} else {
		 
		ret = vfio_assign_device_set(&vdev->vdev, pdev->bus);
	}

	if (ret)
		return ret;
	ret = vfio_pci_vf_init(vdev);
	if (ret)
		return ret;
	ret = vfio_pci_vga_init(vdev);
	if (ret)
		goto out_vf;

	vfio_pci_probe_power_state(vdev);

	 
	vfio_pci_set_power_state(vdev, PCI_D0);

	dev->driver->pm = &vfio_pci_core_pm_ops;
	pm_runtime_allow(dev);
	if (!disable_idle_d3)
		pm_runtime_put(dev);

	ret = vfio_register_group_dev(&vdev->vdev);
	if (ret)
		goto out_power;
	return 0;

out_power:
	if (!disable_idle_d3)
		pm_runtime_get_noresume(dev);

	pm_runtime_forbid(dev);
out_vf:
	vfio_pci_vf_uninit(vdev);
	return ret;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_register_device);

void vfio_pci_core_unregister_device(struct vfio_pci_core_device *vdev)
{
	vfio_pci_core_sriov_configure(vdev, 0);

	vfio_unregister_group_dev(&vdev->vdev);

	vfio_pci_vf_uninit(vdev);
	vfio_pci_vga_uninit(vdev);

	if (!disable_idle_d3)
		pm_runtime_get_noresume(&vdev->pdev->dev);

	pm_runtime_forbid(&vdev->pdev->dev);
}
EXPORT_SYMBOL_GPL(vfio_pci_core_unregister_device);

pci_ers_result_t vfio_pci_core_aer_err_detected(struct pci_dev *pdev,
						pci_channel_state_t state)
{
	struct vfio_pci_core_device *vdev = dev_get_drvdata(&pdev->dev);

	mutex_lock(&vdev->igate);

	if (vdev->err_trigger)
		eventfd_signal(vdev->err_trigger, 1);

	mutex_unlock(&vdev->igate);

	return PCI_ERS_RESULT_CAN_RECOVER;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_aer_err_detected);

int vfio_pci_core_sriov_configure(struct vfio_pci_core_device *vdev,
				  int nr_virtfn)
{
	struct pci_dev *pdev = vdev->pdev;
	int ret = 0;

	device_lock_assert(&pdev->dev);

	if (nr_virtfn) {
		mutex_lock(&vfio_pci_sriov_pfs_mutex);
		 
		if (!list_empty(&vdev->sriov_pfs_item)) {
			ret = -EINVAL;
			goto out_unlock;
		}
		list_add_tail(&vdev->sriov_pfs_item, &vfio_pci_sriov_pfs);
		mutex_unlock(&vfio_pci_sriov_pfs_mutex);

		 
		ret = pm_runtime_resume_and_get(&pdev->dev);
		if (ret)
			goto out_del;

		down_write(&vdev->memory_lock);
		vfio_pci_set_power_state(vdev, PCI_D0);
		ret = pci_enable_sriov(pdev, nr_virtfn);
		up_write(&vdev->memory_lock);
		if (ret) {
			pm_runtime_put(&pdev->dev);
			goto out_del;
		}
		return nr_virtfn;
	}

	if (pci_num_vf(pdev)) {
		pci_disable_sriov(pdev);
		pm_runtime_put(&pdev->dev);
	}

out_del:
	mutex_lock(&vfio_pci_sriov_pfs_mutex);
	list_del_init(&vdev->sriov_pfs_item);
out_unlock:
	mutex_unlock(&vfio_pci_sriov_pfs_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_sriov_configure);

const struct pci_error_handlers vfio_pci_core_err_handlers = {
	.error_detected = vfio_pci_core_aer_err_detected,
};
EXPORT_SYMBOL_GPL(vfio_pci_core_err_handlers);

static bool vfio_dev_in_groups(struct vfio_device *vdev,
			       struct vfio_pci_group_info *groups)
{
	unsigned int i;

	if (!groups)
		return false;

	for (i = 0; i < groups->count; i++)
		if (vfio_file_has_dev(groups->files[i], vdev))
			return true;
	return false;
}

static int vfio_pci_is_device_in_set(struct pci_dev *pdev, void *data)
{
	struct vfio_device_set *dev_set = data;

	return vfio_find_device_in_devset(dev_set, &pdev->dev) ? 0 : -ENODEV;
}

 
static struct pci_dev *
vfio_pci_dev_set_resettable(struct vfio_device_set *dev_set)
{
	struct pci_dev *pdev;

	lockdep_assert_held(&dev_set->lock);

	 
	pdev = list_first_entry(&dev_set->device_list,
				struct vfio_pci_core_device,
				vdev.dev_set_list)->pdev;

	 
	if (pci_probe_reset_slot(pdev->slot) && pci_probe_reset_bus(pdev->bus))
		return NULL;

	if (vfio_pci_for_each_slot_or_bus(pdev, vfio_pci_is_device_in_set,
					  dev_set,
					  !pci_probe_reset_slot(pdev->slot)))
		return NULL;
	return pdev;
}

static int vfio_pci_dev_set_pm_runtime_get(struct vfio_device_set *dev_set)
{
	struct vfio_pci_core_device *cur;
	int ret;

	list_for_each_entry(cur, &dev_set->device_list, vdev.dev_set_list) {
		ret = pm_runtime_resume_and_get(&cur->pdev->dev);
		if (ret)
			goto unwind;
	}

	return 0;

unwind:
	list_for_each_entry_continue_reverse(cur, &dev_set->device_list,
					     vdev.dev_set_list)
		pm_runtime_put(&cur->pdev->dev);

	return ret;
}

 
static int vfio_pci_dev_set_hot_reset(struct vfio_device_set *dev_set,
				      struct vfio_pci_group_info *groups,
				      struct iommufd_ctx *iommufd_ctx)
{
	struct vfio_pci_core_device *cur_mem;
	struct vfio_pci_core_device *cur_vma;
	struct vfio_pci_core_device *cur;
	struct pci_dev *pdev;
	bool is_mem = true;
	int ret;

	mutex_lock(&dev_set->lock);
	cur_mem = list_first_entry(&dev_set->device_list,
				   struct vfio_pci_core_device,
				   vdev.dev_set_list);

	pdev = vfio_pci_dev_set_resettable(dev_set);
	if (!pdev) {
		ret = -EINVAL;
		goto err_unlock;
	}

	 
	ret = vfio_pci_dev_set_pm_runtime_get(dev_set);
	if (ret)
		goto err_unlock;

	list_for_each_entry(cur_vma, &dev_set->device_list, vdev.dev_set_list) {
		bool owned;

		 
		if (iommufd_ctx) {
			int devid = vfio_iommufd_get_dev_id(&cur_vma->vdev,
							    iommufd_ctx);

			owned = (devid > 0 || devid == -ENOENT);
		} else {
			owned = vfio_dev_in_groups(&cur_vma->vdev, groups);
		}

		if (!owned) {
			ret = -EINVAL;
			goto err_undo;
		}

		 
		if (!vfio_pci_zap_and_vma_lock(cur_vma, true)) {
			ret = -EBUSY;
			goto err_undo;
		}
	}
	cur_vma = NULL;

	list_for_each_entry(cur_mem, &dev_set->device_list, vdev.dev_set_list) {
		if (!down_write_trylock(&cur_mem->memory_lock)) {
			ret = -EBUSY;
			goto err_undo;
		}
		mutex_unlock(&cur_mem->vma_lock);
	}
	cur_mem = NULL;

	 
	list_for_each_entry(cur, &dev_set->device_list, vdev.dev_set_list)
		vfio_pci_set_power_state(cur, PCI_D0);

	ret = pci_reset_bus(pdev);

err_undo:
	list_for_each_entry(cur, &dev_set->device_list, vdev.dev_set_list) {
		if (cur == cur_mem)
			is_mem = false;
		if (cur == cur_vma)
			break;
		if (is_mem)
			up_write(&cur->memory_lock);
		else
			mutex_unlock(&cur->vma_lock);
	}

	list_for_each_entry(cur, &dev_set->device_list, vdev.dev_set_list)
		pm_runtime_put(&cur->pdev->dev);
err_unlock:
	mutex_unlock(&dev_set->lock);
	return ret;
}

static bool vfio_pci_dev_set_needs_reset(struct vfio_device_set *dev_set)
{
	struct vfio_pci_core_device *cur;
	bool needs_reset = false;

	 
	if (vfio_device_set_open_count(dev_set) > 1)
		return false;

	list_for_each_entry(cur, &dev_set->device_list, vdev.dev_set_list)
		needs_reset |= cur->needs_reset;
	return needs_reset;
}

 
static void vfio_pci_dev_set_try_reset(struct vfio_device_set *dev_set)
{
	struct vfio_pci_core_device *cur;
	struct pci_dev *pdev;
	bool reset_done = false;

	if (!vfio_pci_dev_set_needs_reset(dev_set))
		return;

	pdev = vfio_pci_dev_set_resettable(dev_set);
	if (!pdev)
		return;

	 
	if (!disable_idle_d3 && vfio_pci_dev_set_pm_runtime_get(dev_set))
		return;

	if (!pci_reset_bus(pdev))
		reset_done = true;

	list_for_each_entry(cur, &dev_set->device_list, vdev.dev_set_list) {
		if (reset_done)
			cur->needs_reset = false;

		if (!disable_idle_d3)
			pm_runtime_put(&cur->pdev->dev);
	}
}

void vfio_pci_core_set_params(bool is_nointxmask, bool is_disable_vga,
			      bool is_disable_idle_d3)
{
	nointxmask = is_nointxmask;
	disable_vga = is_disable_vga;
	disable_idle_d3 = is_disable_idle_d3;
}
EXPORT_SYMBOL_GPL(vfio_pci_core_set_params);

static void vfio_pci_core_cleanup(void)
{
	vfio_pci_uninit_perm_bits();
}

static int __init vfio_pci_core_init(void)
{
	 
	return vfio_pci_init_perm_bits();
}

module_init(vfio_pci_core_init);
module_exit(vfio_pci_core_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
