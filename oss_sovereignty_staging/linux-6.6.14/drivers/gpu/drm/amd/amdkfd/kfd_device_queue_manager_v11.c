 

#include "kfd_device_queue_manager.h"
#include "gc/gc_11_0_0_offset.h"
#include "gc/gc_11_0_0_sh_mask.h"
#include "soc21_enum.h"

static int update_qpd_v11(struct device_queue_manager *dqm,
			 struct qcm_process_device *qpd);
static void init_sdma_vm_v11(struct device_queue_manager *dqm, struct queue *q,
			    struct qcm_process_device *qpd);

void device_queue_manager_init_v11(
	struct device_queue_manager_asic_ops *asic_ops)
{
	asic_ops->update_qpd = update_qpd_v11;
	asic_ops->init_sdma_vm = init_sdma_vm_v11;
	asic_ops->mqd_manager_init = mqd_manager_init_v11;
}

static uint32_t compute_sh_mem_bases_64bit(struct kfd_process_device *pdd)
{
	uint32_t shared_base = pdd->lds_base >> 48;
	uint32_t private_base = pdd->scratch_base >> 48;

	return (shared_base << SH_MEM_BASES__SHARED_BASE__SHIFT) |
		private_base;
}

static int update_qpd_v11(struct device_queue_manager *dqm,
			 struct qcm_process_device *qpd)
{
	struct kfd_process_device *pdd;

	pdd = qpd_to_pdd(qpd);

	 
	if (qpd->sh_mem_config == 0) {
		qpd->sh_mem_config =
			(SH_MEM_ALIGNMENT_MODE_UNALIGNED <<
				SH_MEM_CONFIG__ALIGNMENT_MODE__SHIFT) |
			(3 << SH_MEM_CONFIG__INITIAL_INST_PREFETCH__SHIFT);

		qpd->sh_mem_ape1_limit = 0;
		qpd->sh_mem_ape1_base = 0;
	}

	qpd->sh_mem_bases = compute_sh_mem_bases_64bit(pdd);

	pr_debug("sh_mem_bases 0x%X\n", qpd->sh_mem_bases);

	return 0;
}

static void init_sdma_vm_v11(struct device_queue_manager *dqm, struct queue *q,
			    struct qcm_process_device *qpd)
{
	 
	q->properties.sdma_vm_addr = 0;
}
