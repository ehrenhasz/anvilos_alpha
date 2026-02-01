
 

#include <linux/device.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <uapi/linux/kfd_ioctl.h>
#include <linux/time.h>
#include "kfd_priv.h"
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/processor.h>

 

#define MAKE_GPUVM_APP_BASE_VI(gpu_num) \
	(((uint64_t)(gpu_num) << 61) + 0x1000000000000L)

#define MAKE_GPUVM_APP_LIMIT(base, size) \
	(((uint64_t)(base) & 0xFFFFFF0000000000UL) + (size) - 1)

#define MAKE_SCRATCH_APP_BASE_VI() \
	(((uint64_t)(0x1UL) << 61) + 0x100000000L)

#define MAKE_SCRATCH_APP_LIMIT(base) \
	(((uint64_t)base & 0xFFFFFFFF00000000UL) | 0xFFFFFFFF)

#define MAKE_LDS_APP_BASE_VI() \
	(((uint64_t)(0x1UL) << 61) + 0x0)
#define MAKE_LDS_APP_LIMIT(base) \
	(((uint64_t)(base) & 0xFFFFFFFF00000000UL) | 0xFFFFFFFF)

 
#define MAKE_LDS_APP_BASE_V9() ((uint64_t)(0x1UL) << 48)
#define MAKE_SCRATCH_APP_BASE_V9() ((uint64_t)(0x2UL) << 48)

 
#define SVM_USER_BASE (u64)(KFD_CWSR_TBA_TMA_SIZE + 2*PAGE_SIZE)
#define SVM_CWSR_BASE (SVM_USER_BASE - KFD_CWSR_TBA_TMA_SIZE)
#define SVM_IB_BASE   (SVM_CWSR_BASE - PAGE_SIZE)

static void kfd_init_apertures_vi(struct kfd_process_device *pdd, uint8_t id)
{
	 
	pdd->lds_base = MAKE_LDS_APP_BASE_VI();
	pdd->lds_limit = MAKE_LDS_APP_LIMIT(pdd->lds_base);

	 
	pdd->gpuvm_base = SVM_USER_BASE;
	pdd->gpuvm_limit =
		pdd->dev->kfd->shared_resources.gpuvm_size - 1;

	pdd->scratch_base = MAKE_SCRATCH_APP_BASE_VI();
	pdd->scratch_limit = MAKE_SCRATCH_APP_LIMIT(pdd->scratch_base);
}

static void kfd_init_apertures_v9(struct kfd_process_device *pdd, uint8_t id)
{
	pdd->lds_base = MAKE_LDS_APP_BASE_V9();
	pdd->lds_limit = MAKE_LDS_APP_LIMIT(pdd->lds_base);

         
        pdd->gpuvm_base = SVM_USER_BASE;
	pdd->gpuvm_limit =
		pdd->dev->kfd->shared_resources.gpuvm_size - 1;

	pdd->scratch_base = MAKE_SCRATCH_APP_BASE_V9();
	pdd->scratch_limit = MAKE_SCRATCH_APP_LIMIT(pdd->scratch_base);
}

int kfd_init_apertures(struct kfd_process *process)
{
	uint8_t id  = 0;
	struct kfd_node *dev;
	struct kfd_process_device *pdd;

	 
	while (kfd_topology_enum_kfd_devices(id, &dev) == 0) {
		if (!dev || kfd_devcgroup_check_permission(dev)) {
			 
			id++;
			continue;
		}

		pdd = kfd_create_process_device_data(dev, process);
		if (!pdd) {
			pr_err("Failed to create process device data\n");
			return -ENOMEM;
		}
		 
		if (process->is_32bit_user_mode) {
			pdd->lds_base = pdd->lds_limit = 0;
			pdd->gpuvm_base = pdd->gpuvm_limit = 0;
			pdd->scratch_base = pdd->scratch_limit = 0;
		} else {
			switch (dev->adev->asic_type) {
			case CHIP_KAVERI:
			case CHIP_HAWAII:
			case CHIP_CARRIZO:
			case CHIP_TONGA:
			case CHIP_FIJI:
			case CHIP_POLARIS10:
			case CHIP_POLARIS11:
			case CHIP_POLARIS12:
			case CHIP_VEGAM:
				kfd_init_apertures_vi(pdd, id);
				break;
			default:
				if (KFD_GC_VERSION(dev) >= IP_VERSION(9, 0, 1))
					kfd_init_apertures_v9(pdd, id);
				else {
					WARN(1, "Unexpected ASIC family %u",
					     dev->adev->asic_type);
					return -EINVAL;
				}
			}

                         
                        pdd->qpd.cwsr_base = SVM_CWSR_BASE;
                        pdd->qpd.ib_base = SVM_IB_BASE;
		}

		dev_dbg(kfd_device, "node id %u\n", id);
		dev_dbg(kfd_device, "gpu id %u\n", pdd->dev->id);
		dev_dbg(kfd_device, "lds_base %llX\n", pdd->lds_base);
		dev_dbg(kfd_device, "lds_limit %llX\n", pdd->lds_limit);
		dev_dbg(kfd_device, "gpuvm_base %llX\n", pdd->gpuvm_base);
		dev_dbg(kfd_device, "gpuvm_limit %llX\n", pdd->gpuvm_limit);
		dev_dbg(kfd_device, "scratch_base %llX\n", pdd->scratch_base);
		dev_dbg(kfd_device, "scratch_limit %llX\n", pdd->scratch_limit);

		id++;
	}

	return 0;
}
