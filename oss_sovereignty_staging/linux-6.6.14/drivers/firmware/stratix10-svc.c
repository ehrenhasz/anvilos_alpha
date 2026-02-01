
 

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/firmware/intel/stratix10-smc.h>
#include <linux/firmware/intel/stratix10-svc-client.h>
#include <linux/types.h>

 
#define SVC_NUM_DATA_IN_FIFO			32
#define SVC_NUM_CHANNEL				3
#define FPGA_CONFIG_DATA_CLAIM_TIMEOUT_MS	200
#define FPGA_CONFIG_STATUS_TIMEOUT_SEC		30
#define BYTE_TO_WORD_SIZE              4

 
#define STRATIX10_RSU				"stratix10-rsu"
#define INTEL_FCS				"intel-fcs"

typedef void (svc_invoke_fn)(unsigned long, unsigned long, unsigned long,
			     unsigned long, unsigned long, unsigned long,
			     unsigned long, unsigned long,
			     struct arm_smccc_res *);
struct stratix10_svc_chan;

 
struct stratix10_svc {
	struct platform_device *stratix10_svc_rsu;
	struct platform_device *intel_svc_fcs;
};

 
struct stratix10_svc_sh_memory {
	struct completion sync_complete;
	unsigned long addr;
	unsigned long size;
	svc_invoke_fn *invoke_fn;
};

 
struct stratix10_svc_data_mem {
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
	struct list_head node;
};

 
struct stratix10_svc_data {
	struct stratix10_svc_chan *chan;
	phys_addr_t paddr;
	size_t size;
	phys_addr_t paddr_output;
	size_t size_output;
	u32 command;
	u32 flag;
	u64 arg[3];
};

 
struct stratix10_svc_controller {
	struct device *dev;
	struct stratix10_svc_chan *chans;
	int num_chans;
	int num_active_client;
	struct list_head node;
	struct gen_pool *genpool;
	struct task_struct *task;
	struct kfifo svc_fifo;
	struct completion complete_status;
	spinlock_t svc_fifo_lock;
	svc_invoke_fn *invoke_fn;
};

 
struct stratix10_svc_chan {
	struct stratix10_svc_controller *ctrl;
	struct stratix10_svc_client *scl;
	char *name;
	spinlock_t lock;
};

static LIST_HEAD(svc_ctrl);
static LIST_HEAD(svc_data_mem);

 
static void *svc_pa_to_va(unsigned long addr)
{
	struct stratix10_svc_data_mem *pmem;

	pr_debug("claim back P-addr=0x%016x\n", (unsigned int)addr);
	list_for_each_entry(pmem, &svc_data_mem, node)
		if (pmem->paddr == addr)
			return pmem->vaddr;

	 
	return NULL;
}

 
static void svc_thread_cmd_data_claim(struct stratix10_svc_controller *ctrl,
				      struct stratix10_svc_data *p_data,
				      struct stratix10_svc_cb_data *cb_data)
{
	struct arm_smccc_res res;
	unsigned long timeout;

	reinit_completion(&ctrl->complete_status);
	timeout = msecs_to_jiffies(FPGA_CONFIG_DATA_CLAIM_TIMEOUT_MS);

	pr_debug("%s: claim back the submitted buffer\n", __func__);
	do {
		ctrl->invoke_fn(INTEL_SIP_SMC_FPGA_CONFIG_COMPLETED_WRITE,
				0, 0, 0, 0, 0, 0, 0, &res);

		if (res.a0 == INTEL_SIP_SMC_STATUS_OK) {
			if (!res.a1) {
				complete(&ctrl->complete_status);
				break;
			}
			cb_data->status = BIT(SVC_STATUS_BUFFER_DONE);
			cb_data->kaddr1 = svc_pa_to_va(res.a1);
			cb_data->kaddr2 = (res.a2) ?
					  svc_pa_to_va(res.a2) : NULL;
			cb_data->kaddr3 = (res.a3) ?
					  svc_pa_to_va(res.a3) : NULL;
			p_data->chan->scl->receive_cb(p_data->chan->scl,
						      cb_data);
		} else {
			pr_debug("%s: secure world busy, polling again\n",
				 __func__);
		}
	} while (res.a0 == INTEL_SIP_SMC_STATUS_OK ||
		 res.a0 == INTEL_SIP_SMC_STATUS_BUSY ||
		 wait_for_completion_timeout(&ctrl->complete_status, timeout));
}

 
static void svc_thread_cmd_config_status(struct stratix10_svc_controller *ctrl,
					 struct stratix10_svc_data *p_data,
					 struct stratix10_svc_cb_data *cb_data)
{
	struct arm_smccc_res res;
	int count_in_sec;
	unsigned long a0, a1, a2;

	cb_data->kaddr1 = NULL;
	cb_data->kaddr2 = NULL;
	cb_data->kaddr3 = NULL;
	cb_data->status = BIT(SVC_STATUS_ERROR);

	pr_debug("%s: polling config status\n", __func__);

	a0 = INTEL_SIP_SMC_FPGA_CONFIG_ISDONE;
	a1 = (unsigned long)p_data->paddr;
	a2 = (unsigned long)p_data->size;

	if (p_data->command == COMMAND_POLL_SERVICE_STATUS)
		a0 = INTEL_SIP_SMC_SERVICE_COMPLETED;

	count_in_sec = FPGA_CONFIG_STATUS_TIMEOUT_SEC;
	while (count_in_sec) {
		ctrl->invoke_fn(a0, a1, a2, 0, 0, 0, 0, 0, &res);
		if ((res.a0 == INTEL_SIP_SMC_STATUS_OK) ||
		    (res.a0 == INTEL_SIP_SMC_STATUS_ERROR) ||
		    (res.a0 == INTEL_SIP_SMC_STATUS_REJECTED))
			break;

		 
		msleep(1000);
		count_in_sec--;
	}

	if (!count_in_sec) {
		pr_err("%s: poll status timeout\n", __func__);
		cb_data->status = BIT(SVC_STATUS_BUSY);
	} else if (res.a0 == INTEL_SIP_SMC_STATUS_OK) {
		cb_data->status = BIT(SVC_STATUS_COMPLETED);
		cb_data->kaddr2 = (res.a2) ?
				  svc_pa_to_va(res.a2) : NULL;
		cb_data->kaddr3 = (res.a3) ? &res.a3 : NULL;
	} else {
		pr_err("%s: poll status error\n", __func__);
		cb_data->kaddr1 = &res.a1;
		cb_data->kaddr2 = (res.a2) ?
				  svc_pa_to_va(res.a2) : NULL;
		cb_data->kaddr3 = (res.a3) ? &res.a3 : NULL;
		cb_data->status = BIT(SVC_STATUS_ERROR);
	}

	p_data->chan->scl->receive_cb(p_data->chan->scl, cb_data);
}

 
static void svc_thread_recv_status_ok(struct stratix10_svc_data *p_data,
				      struct stratix10_svc_cb_data *cb_data,
				      struct arm_smccc_res res)
{
	cb_data->kaddr1 = NULL;
	cb_data->kaddr2 = NULL;
	cb_data->kaddr3 = NULL;

	switch (p_data->command) {
	case COMMAND_RECONFIG:
	case COMMAND_RSU_UPDATE:
	case COMMAND_RSU_NOTIFY:
	case COMMAND_FCS_REQUEST_SERVICE:
	case COMMAND_FCS_SEND_CERTIFICATE:
	case COMMAND_FCS_DATA_ENCRYPTION:
	case COMMAND_FCS_DATA_DECRYPTION:
		cb_data->status = BIT(SVC_STATUS_OK);
		break;
	case COMMAND_RECONFIG_DATA_SUBMIT:
		cb_data->status = BIT(SVC_STATUS_BUFFER_SUBMITTED);
		break;
	case COMMAND_RECONFIG_STATUS:
		cb_data->status = BIT(SVC_STATUS_COMPLETED);
		break;
	case COMMAND_RSU_RETRY:
	case COMMAND_RSU_MAX_RETRY:
	case COMMAND_RSU_DCMF_STATUS:
	case COMMAND_FIRMWARE_VERSION:
		cb_data->status = BIT(SVC_STATUS_OK);
		cb_data->kaddr1 = &res.a1;
		break;
	case COMMAND_SMC_SVC_VERSION:
		cb_data->status = BIT(SVC_STATUS_OK);
		cb_data->kaddr1 = &res.a1;
		cb_data->kaddr2 = &res.a2;
		break;
	case COMMAND_RSU_DCMF_VERSION:
		cb_data->status = BIT(SVC_STATUS_OK);
		cb_data->kaddr1 = &res.a1;
		cb_data->kaddr2 = &res.a2;
		break;
	case COMMAND_FCS_RANDOM_NUMBER_GEN:
	case COMMAND_FCS_GET_PROVISION_DATA:
	case COMMAND_POLL_SERVICE_STATUS:
		cb_data->status = BIT(SVC_STATUS_OK);
		cb_data->kaddr1 = &res.a1;
		cb_data->kaddr2 = svc_pa_to_va(res.a2);
		cb_data->kaddr3 = &res.a3;
		break;
	case COMMAND_MBOX_SEND_CMD:
		cb_data->status = BIT(SVC_STATUS_OK);
		cb_data->kaddr1 = &res.a1;
		 
		res.a2 = res.a2 * BYTE_TO_WORD_SIZE;
		cb_data->kaddr2 = &res.a2;
		break;
	default:
		pr_warn("it shouldn't happen\n");
		break;
	}

	pr_debug("%s: call receive_cb\n", __func__);
	p_data->chan->scl->receive_cb(p_data->chan->scl, cb_data);
}

 
static int svc_normal_to_secure_thread(void *data)
{
	struct stratix10_svc_controller
			*ctrl = (struct stratix10_svc_controller *)data;
	struct stratix10_svc_data *pdata;
	struct stratix10_svc_cb_data *cbdata;
	struct arm_smccc_res res;
	unsigned long a0, a1, a2, a3, a4, a5, a6, a7;
	int ret_fifo = 0;

	pdata =  kmalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	cbdata = kmalloc(sizeof(*cbdata), GFP_KERNEL);
	if (!cbdata) {
		kfree(pdata);
		return -ENOMEM;
	}

	 
	a0 = INTEL_SIP_SMC_FPGA_CONFIG_LOOPBACK;
	a1 = 0;
	a2 = 0;
	a3 = 0;
	a4 = 0;
	a5 = 0;
	a6 = 0;
	a7 = 0;

	pr_debug("smc_hvc_shm_thread is running\n");

	while (!kthread_should_stop()) {
		ret_fifo = kfifo_out_spinlocked(&ctrl->svc_fifo,
						pdata, sizeof(*pdata),
						&ctrl->svc_fifo_lock);

		if (!ret_fifo)
			continue;

		pr_debug("get from FIFO pa=0x%016x, command=%u, size=%u\n",
			 (unsigned int)pdata->paddr, pdata->command,
			 (unsigned int)pdata->size);

		switch (pdata->command) {
		case COMMAND_RECONFIG_DATA_CLAIM:
			svc_thread_cmd_data_claim(ctrl, pdata, cbdata);
			continue;
		case COMMAND_RECONFIG:
			a0 = INTEL_SIP_SMC_FPGA_CONFIG_START;
			pr_debug("conf_type=%u\n", (unsigned int)pdata->flag);
			a1 = pdata->flag;
			a2 = 0;
			break;
		case COMMAND_RECONFIG_DATA_SUBMIT:
			a0 = INTEL_SIP_SMC_FPGA_CONFIG_WRITE;
			a1 = (unsigned long)pdata->paddr;
			a2 = (unsigned long)pdata->size;
			break;
		case COMMAND_RECONFIG_STATUS:
			a0 = INTEL_SIP_SMC_FPGA_CONFIG_ISDONE;
			a1 = 0;
			a2 = 0;
			break;
		case COMMAND_RSU_STATUS:
			a0 = INTEL_SIP_SMC_RSU_STATUS;
			a1 = 0;
			a2 = 0;
			break;
		case COMMAND_RSU_UPDATE:
			a0 = INTEL_SIP_SMC_RSU_UPDATE;
			a1 = pdata->arg[0];
			a2 = 0;
			break;
		case COMMAND_RSU_NOTIFY:
			a0 = INTEL_SIP_SMC_RSU_NOTIFY;
			a1 = pdata->arg[0];
			a2 = 0;
			break;
		case COMMAND_RSU_RETRY:
			a0 = INTEL_SIP_SMC_RSU_RETRY_COUNTER;
			a1 = 0;
			a2 = 0;
			break;
		case COMMAND_RSU_MAX_RETRY:
			a0 = INTEL_SIP_SMC_RSU_MAX_RETRY;
			a1 = 0;
			a2 = 0;
			break;
		case COMMAND_RSU_DCMF_VERSION:
			a0 = INTEL_SIP_SMC_RSU_DCMF_VERSION;
			a1 = 0;
			a2 = 0;
			break;
		case COMMAND_FIRMWARE_VERSION:
			a0 = INTEL_SIP_SMC_FIRMWARE_VERSION;
			a1 = 0;
			a2 = 0;
			break;

		 
		case COMMAND_FCS_DATA_ENCRYPTION:
			a0 = INTEL_SIP_SMC_FCS_CRYPTION;
			a1 = 1;
			a2 = (unsigned long)pdata->paddr;
			a3 = (unsigned long)pdata->size;
			a4 = (unsigned long)pdata->paddr_output;
			a5 = (unsigned long)pdata->size_output;
			break;
		case COMMAND_FCS_DATA_DECRYPTION:
			a0 = INTEL_SIP_SMC_FCS_CRYPTION;
			a1 = 0;
			a2 = (unsigned long)pdata->paddr;
			a3 = (unsigned long)pdata->size;
			a4 = (unsigned long)pdata->paddr_output;
			a5 = (unsigned long)pdata->size_output;
			break;
		case COMMAND_FCS_RANDOM_NUMBER_GEN:
			a0 = INTEL_SIP_SMC_FCS_RANDOM_NUMBER;
			a1 = (unsigned long)pdata->paddr;
			a2 = 0;
			break;
		case COMMAND_FCS_REQUEST_SERVICE:
			a0 = INTEL_SIP_SMC_FCS_SERVICE_REQUEST;
			a1 = (unsigned long)pdata->paddr;
			a2 = (unsigned long)pdata->size;
			break;
		case COMMAND_FCS_SEND_CERTIFICATE:
			a0 = INTEL_SIP_SMC_FCS_SEND_CERTIFICATE;
			a1 = (unsigned long)pdata->paddr;
			a2 = (unsigned long)pdata->size;
			break;
		case COMMAND_FCS_GET_PROVISION_DATA:
			a0 = INTEL_SIP_SMC_FCS_GET_PROVISION_DATA;
			a1 = (unsigned long)pdata->paddr;
			a2 = 0;
			break;

		 
		case COMMAND_POLL_SERVICE_STATUS:
			a0 = INTEL_SIP_SMC_SERVICE_COMPLETED;
			a1 = (unsigned long)pdata->paddr;
			a2 = (unsigned long)pdata->size;
			break;
		case COMMAND_RSU_DCMF_STATUS:
			a0 = INTEL_SIP_SMC_RSU_DCMF_STATUS;
			a1 = 0;
			a2 = 0;
			break;
		case COMMAND_SMC_SVC_VERSION:
			a0 = INTEL_SIP_SMC_SVC_VERSION;
			a1 = 0;
			a2 = 0;
			break;
		case COMMAND_MBOX_SEND_CMD:
			a0 = INTEL_SIP_SMC_MBOX_SEND_CMD;
			a1 = pdata->arg[0];
			a2 = (unsigned long)pdata->paddr;
			a3 = (unsigned long)pdata->size / BYTE_TO_WORD_SIZE;
			a4 = pdata->arg[1];
			a5 = (unsigned long)pdata->paddr_output;
			a6 = (unsigned long)pdata->size_output / BYTE_TO_WORD_SIZE;
			break;
		default:
			pr_warn("it shouldn't happen\n");
			break;
		}
		pr_debug("%s: before SMC call -- a0=0x%016x a1=0x%016x",
			 __func__,
			 (unsigned int)a0,
			 (unsigned int)a1);
		pr_debug(" a2=0x%016x\n", (unsigned int)a2);
		pr_debug(" a3=0x%016x\n", (unsigned int)a3);
		pr_debug(" a4=0x%016x\n", (unsigned int)a4);
		pr_debug(" a5=0x%016x\n", (unsigned int)a5);
		ctrl->invoke_fn(a0, a1, a2, a3, a4, a5, a6, a7, &res);

		pr_debug("%s: after SMC call -- res.a0=0x%016x",
			 __func__, (unsigned int)res.a0);
		pr_debug(" res.a1=0x%016x, res.a2=0x%016x",
			 (unsigned int)res.a1, (unsigned int)res.a2);
		pr_debug(" res.a3=0x%016x\n", (unsigned int)res.a3);

		if (pdata->command == COMMAND_RSU_STATUS) {
			if (res.a0 == INTEL_SIP_SMC_RSU_ERROR)
				cbdata->status = BIT(SVC_STATUS_ERROR);
			else
				cbdata->status = BIT(SVC_STATUS_OK);

			cbdata->kaddr1 = &res;
			cbdata->kaddr2 = NULL;
			cbdata->kaddr3 = NULL;
			pdata->chan->scl->receive_cb(pdata->chan->scl, cbdata);
			continue;
		}

		switch (res.a0) {
		case INTEL_SIP_SMC_STATUS_OK:
			svc_thread_recv_status_ok(pdata, cbdata, res);
			break;
		case INTEL_SIP_SMC_STATUS_BUSY:
			switch (pdata->command) {
			case COMMAND_RECONFIG_DATA_SUBMIT:
				svc_thread_cmd_data_claim(ctrl,
							  pdata, cbdata);
				break;
			case COMMAND_RECONFIG_STATUS:
			case COMMAND_POLL_SERVICE_STATUS:
				svc_thread_cmd_config_status(ctrl,
							     pdata, cbdata);
				break;
			default:
				pr_warn("it shouldn't happen\n");
				break;
			}
			break;
		case INTEL_SIP_SMC_STATUS_REJECTED:
			pr_debug("%s: STATUS_REJECTED\n", __func__);
			 
			switch (pdata->command) {
			case COMMAND_FCS_REQUEST_SERVICE:
			case COMMAND_FCS_SEND_CERTIFICATE:
			case COMMAND_FCS_GET_PROVISION_DATA:
			case COMMAND_FCS_DATA_ENCRYPTION:
			case COMMAND_FCS_DATA_DECRYPTION:
			case COMMAND_FCS_RANDOM_NUMBER_GEN:
			case COMMAND_MBOX_SEND_CMD:
				cbdata->status = BIT(SVC_STATUS_INVALID_PARAM);
				cbdata->kaddr1 = NULL;
				cbdata->kaddr2 = NULL;
				cbdata->kaddr3 = NULL;
				pdata->chan->scl->receive_cb(pdata->chan->scl,
							     cbdata);
				break;
			}
			break;
		case INTEL_SIP_SMC_STATUS_ERROR:
		case INTEL_SIP_SMC_RSU_ERROR:
			pr_err("%s: STATUS_ERROR\n", __func__);
			cbdata->status = BIT(SVC_STATUS_ERROR);
			cbdata->kaddr1 = &res.a1;
			cbdata->kaddr2 = (res.a2) ?
				svc_pa_to_va(res.a2) : NULL;
			cbdata->kaddr3 = (res.a3) ? &res.a3 : NULL;
			pdata->chan->scl->receive_cb(pdata->chan->scl, cbdata);
			break;
		default:
			pr_warn("Secure firmware doesn't support...\n");

			 
			if ((pdata->command != COMMAND_RSU_UPDATE) &&
				(pdata->command != COMMAND_RSU_STATUS)) {
				cbdata->status =
					BIT(SVC_STATUS_NO_SUPPORT);
				cbdata->kaddr1 = NULL;
				cbdata->kaddr2 = NULL;
				cbdata->kaddr3 = NULL;
				pdata->chan->scl->receive_cb(
					pdata->chan->scl, cbdata);
			}
			break;

		}
	}

	kfree(cbdata);
	kfree(pdata);

	return 0;
}

 
static int svc_normal_to_secure_shm_thread(void *data)
{
	struct stratix10_svc_sh_memory
			*sh_mem = (struct stratix10_svc_sh_memory *)data;
	struct arm_smccc_res res;

	 
	sh_mem->invoke_fn(INTEL_SIP_SMC_FPGA_CONFIG_GET_MEM,
			  0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == INTEL_SIP_SMC_STATUS_OK) {
		sh_mem->addr = res.a1;
		sh_mem->size = res.a2;
	} else {
		pr_err("%s: after SMC call -- res.a0=0x%016x",  __func__,
		       (unsigned int)res.a0);
		sh_mem->addr = 0;
		sh_mem->size = 0;
	}

	complete(&sh_mem->sync_complete);
	return 0;
}

 
static int svc_get_sh_memory(struct platform_device *pdev,
				    struct stratix10_svc_sh_memory *sh_memory)
{
	struct device *dev = &pdev->dev;
	struct task_struct *sh_memory_task;
	unsigned int cpu = 0;

	init_completion(&sh_memory->sync_complete);

	 
	sh_memory_task = kthread_create_on_node(svc_normal_to_secure_shm_thread,
					       (void *)sh_memory,
						cpu_to_node(cpu),
						"svc_smc_hvc_shm_thread");
	if (IS_ERR(sh_memory_task)) {
		dev_err(dev, "fail to create stratix10_svc_smc_shm_thread\n");
		return -EINVAL;
	}

	wake_up_process(sh_memory_task);

	if (!wait_for_completion_timeout(&sh_memory->sync_complete, 10 * HZ)) {
		dev_err(dev,
			"timeout to get sh-memory paras from secure world\n");
		return -ETIMEDOUT;
	}

	if (!sh_memory->addr || !sh_memory->size) {
		dev_err(dev,
			"failed to get shared memory info from secure world\n");
		return -ENOMEM;
	}

	dev_dbg(dev, "SM software provides paddr: 0x%016x, size: 0x%08x\n",
		(unsigned int)sh_memory->addr,
		(unsigned int)sh_memory->size);

	return 0;
}

 
static struct gen_pool *
svc_create_memory_pool(struct platform_device *pdev,
		       struct stratix10_svc_sh_memory *sh_memory)
{
	struct device *dev = &pdev->dev;
	struct gen_pool *genpool;
	unsigned long vaddr;
	phys_addr_t paddr;
	size_t size;
	phys_addr_t begin;
	phys_addr_t end;
	void *va;
	size_t page_mask = PAGE_SIZE - 1;
	int min_alloc_order = 3;
	int ret;

	begin = roundup(sh_memory->addr, PAGE_SIZE);
	end = rounddown(sh_memory->addr + sh_memory->size, PAGE_SIZE);
	paddr = begin;
	size = end - begin;
	va = devm_memremap(dev, paddr, size, MEMREMAP_WC);
	if (IS_ERR(va)) {
		dev_err(dev, "fail to remap shared memory\n");
		return ERR_PTR(-EINVAL);
	}
	vaddr = (unsigned long)va;
	dev_dbg(dev,
		"reserved memory vaddr: %p, paddr: 0x%16x size: 0x%8x\n",
		va, (unsigned int)paddr, (unsigned int)size);
	if ((vaddr & page_mask) || (paddr & page_mask) ||
	    (size & page_mask)) {
		dev_err(dev, "page is not aligned\n");
		return ERR_PTR(-EINVAL);
	}
	genpool = gen_pool_create(min_alloc_order, -1);
	if (!genpool) {
		dev_err(dev, "fail to create genpool\n");
		return ERR_PTR(-ENOMEM);
	}
	gen_pool_set_algo(genpool, gen_pool_best_fit, NULL);
	ret = gen_pool_add_virt(genpool, vaddr, paddr, size, -1);
	if (ret) {
		dev_err(dev, "fail to add memory chunk to the pool\n");
		gen_pool_destroy(genpool);
		return ERR_PTR(ret);
	}

	return genpool;
}

 
static void svc_smccc_smc(unsigned long a0, unsigned long a1,
			  unsigned long a2, unsigned long a3,
			  unsigned long a4, unsigned long a5,
			  unsigned long a6, unsigned long a7,
			  struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

 
static void svc_smccc_hvc(unsigned long a0, unsigned long a1,
			  unsigned long a2, unsigned long a3,
			  unsigned long a4, unsigned long a5,
			  unsigned long a6, unsigned long a7,
			  struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

 
static svc_invoke_fn *get_invoke_func(struct device *dev)
{
	const char *method;

	if (of_property_read_string(dev->of_node, "method", &method)) {
		dev_warn(dev, "missing \"method\" property\n");
		return ERR_PTR(-ENXIO);
	}

	if (!strcmp(method, "smc"))
		return svc_smccc_smc;
	if (!strcmp(method, "hvc"))
		return svc_smccc_hvc;

	dev_warn(dev, "invalid \"method\" property: %s\n", method);

	return ERR_PTR(-EINVAL);
}

 
struct stratix10_svc_chan *stratix10_svc_request_channel_byname(
	struct stratix10_svc_client *client, const char *name)
{
	struct device *dev = client->dev;
	struct stratix10_svc_controller *controller;
	struct stratix10_svc_chan *chan = NULL;
	unsigned long flag;
	int i;

	 
	if (list_empty(&svc_ctrl))
		return ERR_PTR(-EPROBE_DEFER);

	controller = list_first_entry(&svc_ctrl,
				      struct stratix10_svc_controller, node);
	for (i = 0; i < SVC_NUM_CHANNEL; i++) {
		if (!strcmp(controller->chans[i].name, name)) {
			chan = &controller->chans[i];
			break;
		}
	}

	 
	if (i == SVC_NUM_CHANNEL) {
		dev_err(dev, "%s: channel not allocated\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (chan->scl || !try_module_get(controller->dev->driver->owner)) {
		dev_dbg(dev, "%s: svc not free\n", __func__);
		return ERR_PTR(-EBUSY);
	}

	spin_lock_irqsave(&chan->lock, flag);
	chan->scl = client;
	chan->ctrl->num_active_client++;
	spin_unlock_irqrestore(&chan->lock, flag);

	return chan;
}
EXPORT_SYMBOL_GPL(stratix10_svc_request_channel_byname);

 
void stratix10_svc_free_channel(struct stratix10_svc_chan *chan)
{
	unsigned long flag;

	spin_lock_irqsave(&chan->lock, flag);
	chan->scl = NULL;
	chan->ctrl->num_active_client--;
	module_put(chan->ctrl->dev->driver->owner);
	spin_unlock_irqrestore(&chan->lock, flag);
}
EXPORT_SYMBOL_GPL(stratix10_svc_free_channel);

 
int stratix10_svc_send(struct stratix10_svc_chan *chan, void *msg)
{
	struct stratix10_svc_client_msg
		*p_msg = (struct stratix10_svc_client_msg *)msg;
	struct stratix10_svc_data_mem *p_mem;
	struct stratix10_svc_data *p_data;
	int ret = 0;
	unsigned int cpu = 0;

	p_data = kzalloc(sizeof(*p_data), GFP_KERNEL);
	if (!p_data)
		return -ENOMEM;

	 
	if (!chan->ctrl->task) {
		chan->ctrl->task =
			kthread_create_on_node(svc_normal_to_secure_thread,
					      (void *)chan->ctrl,
					      cpu_to_node(cpu),
					      "svc_smc_hvc_thread");
			if (IS_ERR(chan->ctrl->task)) {
				dev_err(chan->ctrl->dev,
					"failed to create svc_smc_hvc_thread\n");
				kfree(p_data);
				return -EINVAL;
			}
		kthread_bind(chan->ctrl->task, cpu);
		wake_up_process(chan->ctrl->task);
	}

	pr_debug("%s: sent P-va=%p, P-com=%x, P-size=%u\n", __func__,
		 p_msg->payload, p_msg->command,
		 (unsigned int)p_msg->payload_length);

	if (list_empty(&svc_data_mem)) {
		if (p_msg->command == COMMAND_RECONFIG) {
			struct stratix10_svc_command_config_type *ct =
				(struct stratix10_svc_command_config_type *)
				p_msg->payload;
			p_data->flag = ct->flags;
		}
	} else {
		list_for_each_entry(p_mem, &svc_data_mem, node)
			if (p_mem->vaddr == p_msg->payload) {
				p_data->paddr = p_mem->paddr;
				p_data->size = p_msg->payload_length;
				break;
			}
		if (p_msg->payload_output) {
			list_for_each_entry(p_mem, &svc_data_mem, node)
				if (p_mem->vaddr == p_msg->payload_output) {
					p_data->paddr_output =
						p_mem->paddr;
					p_data->size_output =
						p_msg->payload_length_output;
					break;
				}
		}
	}

	p_data->command = p_msg->command;
	p_data->arg[0] = p_msg->arg[0];
	p_data->arg[1] = p_msg->arg[1];
	p_data->arg[2] = p_msg->arg[2];
	p_data->size = p_msg->payload_length;
	p_data->chan = chan;
	pr_debug("%s: put to FIFO pa=0x%016x, cmd=%x, size=%u\n", __func__,
	       (unsigned int)p_data->paddr, p_data->command,
	       (unsigned int)p_data->size);
	ret = kfifo_in_spinlocked(&chan->ctrl->svc_fifo, p_data,
				  sizeof(*p_data),
				  &chan->ctrl->svc_fifo_lock);

	kfree(p_data);

	if (!ret)
		return -ENOBUFS;

	return 0;
}
EXPORT_SYMBOL_GPL(stratix10_svc_send);

 
void stratix10_svc_done(struct stratix10_svc_chan *chan)
{
	 
	if (chan->ctrl->task && chan->ctrl->num_active_client <= 1) {
		pr_debug("svc_smc_hvc_shm_thread is stopped\n");
		kthread_stop(chan->ctrl->task);
		chan->ctrl->task = NULL;
	}
}
EXPORT_SYMBOL_GPL(stratix10_svc_done);

 
void *stratix10_svc_allocate_memory(struct stratix10_svc_chan *chan,
				    size_t size)
{
	struct stratix10_svc_data_mem *pmem;
	unsigned long va;
	phys_addr_t pa;
	struct gen_pool *genpool = chan->ctrl->genpool;
	size_t s = roundup(size, 1 << genpool->min_alloc_order);

	pmem = devm_kzalloc(chan->ctrl->dev, sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return ERR_PTR(-ENOMEM);

	va = gen_pool_alloc(genpool, s);
	if (!va)
		return ERR_PTR(-ENOMEM);

	memset((void *)va, 0, s);
	pa = gen_pool_virt_to_phys(genpool, va);

	pmem->vaddr = (void *)va;
	pmem->paddr = pa;
	pmem->size = s;
	list_add_tail(&pmem->node, &svc_data_mem);
	pr_debug("%s: va=%p, pa=0x%016x\n", __func__,
		 pmem->vaddr, (unsigned int)pmem->paddr);

	return (void *)va;
}
EXPORT_SYMBOL_GPL(stratix10_svc_allocate_memory);

 
void stratix10_svc_free_memory(struct stratix10_svc_chan *chan, void *kaddr)
{
	struct stratix10_svc_data_mem *pmem;

	list_for_each_entry(pmem, &svc_data_mem, node)
		if (pmem->vaddr == kaddr) {
			gen_pool_free(chan->ctrl->genpool,
				       (unsigned long)kaddr, pmem->size);
			pmem->vaddr = NULL;
			list_del(&pmem->node);
			return;
		}

	list_del(&svc_data_mem);
}
EXPORT_SYMBOL_GPL(stratix10_svc_free_memory);

static const struct of_device_id stratix10_svc_drv_match[] = {
	{.compatible = "intel,stratix10-svc"},
	{.compatible = "intel,agilex-svc"},
	{},
};

static int stratix10_svc_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stratix10_svc_controller *controller;
	struct stratix10_svc_chan *chans;
	struct gen_pool *genpool;
	struct stratix10_svc_sh_memory *sh_memory;
	struct stratix10_svc *svc;

	svc_invoke_fn *invoke_fn;
	size_t fifo_size;
	int ret;

	 
	invoke_fn = get_invoke_func(dev);
	if (IS_ERR(invoke_fn))
		return -EINVAL;

	sh_memory = devm_kzalloc(dev, sizeof(*sh_memory), GFP_KERNEL);
	if (!sh_memory)
		return -ENOMEM;

	sh_memory->invoke_fn = invoke_fn;
	ret = svc_get_sh_memory(pdev, sh_memory);
	if (ret)
		return ret;

	genpool = svc_create_memory_pool(pdev, sh_memory);
	if (IS_ERR(genpool))
		return PTR_ERR(genpool);

	 
	controller = devm_kzalloc(dev, sizeof(*controller), GFP_KERNEL);
	if (!controller) {
		ret = -ENOMEM;
		goto err_destroy_pool;
	}

	chans = devm_kmalloc_array(dev, SVC_NUM_CHANNEL,
				   sizeof(*chans), GFP_KERNEL | __GFP_ZERO);
	if (!chans) {
		ret = -ENOMEM;
		goto err_destroy_pool;
	}

	controller->dev = dev;
	controller->num_chans = SVC_NUM_CHANNEL;
	controller->num_active_client = 0;
	controller->chans = chans;
	controller->genpool = genpool;
	controller->task = NULL;
	controller->invoke_fn = invoke_fn;
	init_completion(&controller->complete_status);

	fifo_size = sizeof(struct stratix10_svc_data) * SVC_NUM_DATA_IN_FIFO;
	ret = kfifo_alloc(&controller->svc_fifo, fifo_size, GFP_KERNEL);
	if (ret) {
		dev_err(dev, "failed to allocate FIFO\n");
		goto err_destroy_pool;
	}
	spin_lock_init(&controller->svc_fifo_lock);

	chans[0].scl = NULL;
	chans[0].ctrl = controller;
	chans[0].name = SVC_CLIENT_FPGA;
	spin_lock_init(&chans[0].lock);

	chans[1].scl = NULL;
	chans[1].ctrl = controller;
	chans[1].name = SVC_CLIENT_RSU;
	spin_lock_init(&chans[1].lock);

	chans[2].scl = NULL;
	chans[2].ctrl = controller;
	chans[2].name = SVC_CLIENT_FCS;
	spin_lock_init(&chans[2].lock);

	list_add_tail(&controller->node, &svc_ctrl);
	platform_set_drvdata(pdev, controller);

	 
	svc = devm_kzalloc(dev, sizeof(*svc), GFP_KERNEL);
	if (!svc) {
		ret = -ENOMEM;
		goto err_free_kfifo;
	}

	svc->stratix10_svc_rsu = platform_device_alloc(STRATIX10_RSU, 0);
	if (!svc->stratix10_svc_rsu) {
		dev_err(dev, "failed to allocate %s device\n", STRATIX10_RSU);
		ret = -ENOMEM;
		goto err_free_kfifo;
	}

	ret = platform_device_add(svc->stratix10_svc_rsu);
	if (ret) {
		platform_device_put(svc->stratix10_svc_rsu);
		goto err_free_kfifo;
	}

	svc->intel_svc_fcs = platform_device_alloc(INTEL_FCS, 1);
	if (!svc->intel_svc_fcs) {
		dev_err(dev, "failed to allocate %s device\n", INTEL_FCS);
		ret = -ENOMEM;
		goto err_unregister_dev;
	}

	ret = platform_device_add(svc->intel_svc_fcs);
	if (ret) {
		platform_device_put(svc->intel_svc_fcs);
		goto err_unregister_dev;
	}

	dev_set_drvdata(dev, svc);

	pr_info("Intel Service Layer Driver Initialized\n");

	return 0;

err_unregister_dev:
	platform_device_unregister(svc->stratix10_svc_rsu);
err_free_kfifo:
	kfifo_free(&controller->svc_fifo);
err_destroy_pool:
	gen_pool_destroy(genpool);
	return ret;
}

static int stratix10_svc_drv_remove(struct platform_device *pdev)
{
	struct stratix10_svc *svc = dev_get_drvdata(&pdev->dev);
	struct stratix10_svc_controller *ctrl = platform_get_drvdata(pdev);

	platform_device_unregister(svc->intel_svc_fcs);
	platform_device_unregister(svc->stratix10_svc_rsu);

	kfifo_free(&ctrl->svc_fifo);
	if (ctrl->task) {
		kthread_stop(ctrl->task);
		ctrl->task = NULL;
	}
	if (ctrl->genpool)
		gen_pool_destroy(ctrl->genpool);
	list_del(&ctrl->node);

	return 0;
}

static struct platform_driver stratix10_svc_driver = {
	.probe = stratix10_svc_drv_probe,
	.remove = stratix10_svc_drv_remove,
	.driver = {
		.name = "stratix10-svc",
		.of_match_table = stratix10_svc_drv_match,
	},
};

static int __init stratix10_svc_init(void)
{
	struct device_node *fw_np;
	struct device_node *np;
	int ret;

	fw_np = of_find_node_by_name(NULL, "firmware");
	if (!fw_np)
		return -ENODEV;

	np = of_find_matching_node(fw_np, stratix10_svc_drv_match);
	if (!np)
		return -ENODEV;

	of_node_put(np);
	ret = of_platform_populate(fw_np, stratix10_svc_drv_match, NULL, NULL);
	if (ret)
		return ret;

	return platform_driver_register(&stratix10_svc_driver);
}

static void __exit stratix10_svc_exit(void)
{
	return platform_driver_unregister(&stratix10_svc_driver);
}

subsys_initcall(stratix10_svc_init);
module_exit(stratix10_svc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Stratix10 Service Layer Driver");
MODULE_AUTHOR("Richard Gong <richard.gong@intel.com>");
MODULE_ALIAS("platform:stratix10-svc");
