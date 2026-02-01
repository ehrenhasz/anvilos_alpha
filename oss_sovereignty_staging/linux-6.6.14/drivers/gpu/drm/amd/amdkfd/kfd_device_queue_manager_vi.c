
 

#include "kfd_device_queue_manager.h"
#include "gca/gfx_8_0_enum.h"
#include "gca/gfx_8_0_sh_mask.h"
#include "oss/oss_3_0_sh_mask.h"

static bool set_cache_memory_policy_vi(struct device_queue_manager *dqm,
				       struct qcm_process_device *qpd,
				       enum cache_policy default_policy,
				       enum cache_policy alternate_policy,
				       void __user *alternate_aperture_base,
				       uint64_t alternate_aperture_size);
static int update_qpd_vi(struct device_queue_manager *dqm,
			 struct qcm_process_device *qpd);
static void init_sdma_vm(struct device_queue_manager *dqm,
			 struct queue *q,
			 struct qcm_process_device *qpd);

void device_queue_manager_init_vi(
	struct device_queue_manager_asic_ops *asic_ops)
{
	asic_ops->set_cache_memory_policy = set_cache_memory_policy_vi;
	asic_ops->update_qpd = update_qpd_vi;
	asic_ops->init_sdma_vm = init_sdma_vm;
	asic_ops->mqd_manager_init = mqd_manager_init_vi;
}

static uint32_t compute_sh_mem_bases_64bit(unsigned int top_address_nybble)
{
	 

	WARN_ON((top_address_nybble & 1) || top_address_nybble > 0xE ||
		top_address_nybble == 0);

	return top_address_nybble << 12 |
			(top_address_nybble << 12) <<
			SH_MEM_BASES__SHARED_BASE__SHIFT;
}

static bool set_cache_memory_policy_vi(struct device_queue_manager *dqm,
		struct qcm_process_device *qpd,
		enum cache_policy default_policy,
		enum cache_policy alternate_policy,
		void __user *alternate_aperture_base,
		uint64_t alternate_aperture_size)
{
	uint32_t default_mtype;
	uint32_t ape1_mtype;

	default_mtype = (default_policy == cache_policy_coherent) ?
			MTYPE_UC :
			MTYPE_NC;

	ape1_mtype = (alternate_policy == cache_policy_coherent) ?
			MTYPE_UC :
			MTYPE_NC;

	qpd->sh_mem_config =
			SH_MEM_ALIGNMENT_MODE_UNALIGNED <<
				   SH_MEM_CONFIG__ALIGNMENT_MODE__SHIFT |
			default_mtype << SH_MEM_CONFIG__DEFAULT_MTYPE__SHIFT |
			ape1_mtype << SH_MEM_CONFIG__APE1_MTYPE__SHIFT;

	return true;
}

static int update_qpd_vi(struct device_queue_manager *dqm,
			 struct qcm_process_device *qpd)
{
	struct kfd_process_device *pdd;
	unsigned int temp;

	pdd = qpd_to_pdd(qpd);

	 
	if (qpd->sh_mem_config == 0) {
		qpd->sh_mem_config =
				SH_MEM_ALIGNMENT_MODE_UNALIGNED <<
					SH_MEM_CONFIG__ALIGNMENT_MODE__SHIFT |
				MTYPE_UC <<
					SH_MEM_CONFIG__DEFAULT_MTYPE__SHIFT |
				MTYPE_UC <<
					SH_MEM_CONFIG__APE1_MTYPE__SHIFT;

		qpd->sh_mem_ape1_limit = 0;
		qpd->sh_mem_ape1_base = 0;
	}

	 
	temp = get_sh_mem_bases_nybble_64(pdd);
	qpd->sh_mem_bases = compute_sh_mem_bases_64bit(temp);

	pr_debug("sh_mem_bases nybble: 0x%X and register 0x%X\n",
		temp, qpd->sh_mem_bases);

	return 0;
}

static void init_sdma_vm(struct device_queue_manager *dqm,
			 struct queue *q,
			 struct qcm_process_device *qpd)
{
	 
	q->properties.sdma_vm_addr =
		((get_sh_mem_bases_nybble_64(qpd_to_pdd(qpd))) <<
		 SDMA0_RLC0_VIRTUAL_ADDR__SHARED_BASE__SHIFT) &
		SDMA0_RLC0_VIRTUAL_ADDR__SHARED_BASE_MASK;
}
