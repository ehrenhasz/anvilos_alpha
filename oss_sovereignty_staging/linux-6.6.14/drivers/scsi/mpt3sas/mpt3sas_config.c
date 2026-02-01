 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/blkdev.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include "mpt3sas_base.h"

 

 
#define MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT 15

 
#define MPT3_CONFIG_COMMON_SGLFLAGS ((MPI2_SGE_FLAGS_SIMPLE_ELEMENT | \
	MPI2_SGE_FLAGS_LAST_ELEMENT | MPI2_SGE_FLAGS_END_OF_BUFFER \
	| MPI2_SGE_FLAGS_END_OF_LIST) << MPI2_SGE_FLAGS_SHIFT)

 
#define MPT3_CONFIG_COMMON_WRITE_SGLFLAGS ((MPI2_SGE_FLAGS_SIMPLE_ELEMENT | \
	MPI2_SGE_FLAGS_LAST_ELEMENT | MPI2_SGE_FLAGS_END_OF_BUFFER \
	| MPI2_SGE_FLAGS_END_OF_LIST | MPI2_SGE_FLAGS_HOST_TO_IOC) \
	<< MPI2_SGE_FLAGS_SHIFT)

 
struct config_request {
	u16			sz;
	void			*page;
	dma_addr_t		page_dma;
};

 
static void
_config_display_some_debug(struct MPT3SAS_ADAPTER *ioc, u16 smid,
	char *calling_function_name, MPI2DefaultReply_t *mpi_reply)
{
	Mpi2ConfigRequest_t *mpi_request;
	char *desc = NULL;

	mpi_request = mpt3sas_base_get_msg_frame(ioc, smid);
	switch (mpi_request->Header.PageType & MPI2_CONFIG_PAGETYPE_MASK) {
	case MPI2_CONFIG_PAGETYPE_IO_UNIT:
		desc = "io_unit";
		break;
	case MPI2_CONFIG_PAGETYPE_IOC:
		desc = "ioc";
		break;
	case MPI2_CONFIG_PAGETYPE_BIOS:
		desc = "bios";
		break;
	case MPI2_CONFIG_PAGETYPE_RAID_VOLUME:
		desc = "raid_volume";
		break;
	case MPI2_CONFIG_PAGETYPE_MANUFACTURING:
		desc = "manufacturing";
		break;
	case MPI2_CONFIG_PAGETYPE_RAID_PHYSDISK:
		desc = "physdisk";
		break;
	case MPI2_CONFIG_PAGETYPE_EXTENDED:
		switch (mpi_request->ExtPageType) {
		case MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT:
			desc = "sas_io_unit";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER:
			desc = "sas_expander";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE:
			desc = "sas_device";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_SAS_PHY:
			desc = "sas_phy";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_LOG:
			desc = "log";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_ENCLOSURE:
			desc = "enclosure";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_RAID_CONFIG:
			desc = "raid_config";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_DRIVER_MAPPING:
			desc = "driver_mapping";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_SAS_PORT:
			desc = "sas_port";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_EXT_MANUFACTURING:
			desc = "ext_manufacturing";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_PCIE_IO_UNIT:
			desc = "pcie_io_unit";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_PCIE_SWITCH:
			desc = "pcie_switch";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_PCIE_DEVICE:
			desc = "pcie_device";
			break;
		case MPI2_CONFIG_EXTPAGETYPE_PCIE_LINK:
			desc = "pcie_link";
			break;
		}
		break;
	}

	if (!desc)
		return;

	ioc_info(ioc, "%s: %s(%d), action(%d), form(0x%08x), smid(%d)\n",
		 calling_function_name, desc,
		 mpi_request->Header.PageNumber, mpi_request->Action,
		 le32_to_cpu(mpi_request->PageAddress), smid);

	if (!mpi_reply)
		return;

	if (mpi_reply->IOCStatus || mpi_reply->IOCLogInfo)
		ioc_info(ioc, "\tiocstatus(0x%04x), loginfo(0x%08x)\n",
			 le16_to_cpu(mpi_reply->IOCStatus),
			 le32_to_cpu(mpi_reply->IOCLogInfo));
}

 
static int
_config_alloc_config_dma_memory(struct MPT3SAS_ADAPTER *ioc,
	struct config_request *mem)
{
	int r = 0;

	if (mem->sz > ioc->config_page_sz) {
		mem->page = dma_alloc_coherent(&ioc->pdev->dev, mem->sz,
		    &mem->page_dma, GFP_KERNEL);
		if (!mem->page) {
			ioc_err(ioc, "%s: dma_alloc_coherent failed asking for (%d) bytes!!\n",
				__func__, mem->sz);
			r = -ENOMEM;
		}
	} else {  
		mem->page = ioc->config_page;
		mem->page_dma = ioc->config_page_dma;
	}
	ioc->config_vaddr = mem->page;
	return r;
}

 
static void
_config_free_config_dma_memory(struct MPT3SAS_ADAPTER *ioc,
	struct config_request *mem)
{
	if (mem->sz > ioc->config_page_sz)
		dma_free_coherent(&ioc->pdev->dev, mem->sz, mem->page,
		    mem->page_dma);
}

 
u8
mpt3sas_config_done(struct MPT3SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
	u32 reply)
{
	MPI2DefaultReply_t *mpi_reply;

	if (ioc->config_cmds.status == MPT3_CMD_NOT_USED)
		return 1;
	if (ioc->config_cmds.smid != smid)
		return 1;
	ioc->config_cmds.status |= MPT3_CMD_COMPLETE;
	mpi_reply =  mpt3sas_base_get_reply_virt_addr(ioc, reply);
	if (mpi_reply) {
		ioc->config_cmds.status |= MPT3_CMD_REPLY_VALID;
		memcpy(ioc->config_cmds.reply, mpi_reply,
		    mpi_reply->MsgLength*4);
	}
	ioc->config_cmds.status &= ~MPT3_CMD_PENDING;
	if (ioc->logging_level & MPT_DEBUG_CONFIG)
		_config_display_some_debug(ioc, smid, "config_done", mpi_reply);
	ioc->config_cmds.smid = USHRT_MAX;
	complete(&ioc->config_cmds.done);
	return 1;
}

 
static int
_config_request(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigRequest_t
	*mpi_request, Mpi2ConfigReply_t *mpi_reply, int timeout,
	void *config_page, u16 config_page_sz)
{
	u16 smid;
	Mpi2ConfigRequest_t *config_request;
	int r;
	u8 retry_count, issue_host_reset = 0;
	struct config_request mem;
	u32 ioc_status = UINT_MAX;

	mutex_lock(&ioc->config_cmds.mutex);
	if (ioc->config_cmds.status != MPT3_CMD_NOT_USED) {
		ioc_err(ioc, "%s: config_cmd in use\n", __func__);
		mutex_unlock(&ioc->config_cmds.mutex);
		return -EAGAIN;
	}

	retry_count = 0;
	memset(&mem, 0, sizeof(struct config_request));

	mpi_request->VF_ID = 0;  
	mpi_request->VP_ID = 0;

	if (config_page) {
		mpi_request->Header.PageVersion = mpi_reply->Header.PageVersion;
		mpi_request->Header.PageNumber = mpi_reply->Header.PageNumber;
		mpi_request->Header.PageType = mpi_reply->Header.PageType;
		mpi_request->Header.PageLength = mpi_reply->Header.PageLength;
		mpi_request->ExtPageLength = mpi_reply->ExtPageLength;
		mpi_request->ExtPageType = mpi_reply->ExtPageType;
		if (mpi_request->Header.PageLength)
			mem.sz = mpi_request->Header.PageLength * 4;
		else
			mem.sz = le16_to_cpu(mpi_reply->ExtPageLength) * 4;
		r = _config_alloc_config_dma_memory(ioc, &mem);
		if (r != 0)
			goto out;
		if (mpi_request->Action ==
		    MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT ||
		    mpi_request->Action ==
		    MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM) {
			ioc->base_add_sg_single(&mpi_request->PageBufferSGE,
			    MPT3_CONFIG_COMMON_WRITE_SGLFLAGS | mem.sz,
			    mem.page_dma);
			memcpy(mem.page, config_page, min_t(u16, mem.sz,
			    config_page_sz));
		} else {
			memset(config_page, 0, config_page_sz);
			ioc->base_add_sg_single(&mpi_request->PageBufferSGE,
			    MPT3_CONFIG_COMMON_SGLFLAGS | mem.sz, mem.page_dma);
			memset(mem.page, 0, min_t(u16, mem.sz, config_page_sz));
		}
	}

 retry_config:
	if (retry_count) {
		if (retry_count > 2) {  
			r = -EFAULT;
			goto free_mem;
		}
		ioc_info(ioc, "%s: attempting retry (%d)\n",
			 __func__, retry_count);
	}

	r = mpt3sas_wait_for_ioc(ioc, MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT);
	if (r) {
		if (r == -ETIME)
			issue_host_reset = 1;
		goto free_mem;
	}

	smid = mpt3sas_base_get_smid(ioc, ioc->config_cb_idx);
	if (!smid) {
		ioc_err(ioc, "%s: failed obtaining a smid\n", __func__);
		ioc->config_cmds.status = MPT3_CMD_NOT_USED;
		r = -EAGAIN;
		goto free_mem;
	}

	r = 0;
	memset(ioc->config_cmds.reply, 0, sizeof(Mpi2ConfigReply_t));
	ioc->config_cmds.status = MPT3_CMD_PENDING;
	config_request = mpt3sas_base_get_msg_frame(ioc, smid);
	ioc->config_cmds.smid = smid;
	memcpy(config_request, mpi_request, sizeof(Mpi2ConfigRequest_t));
	if (ioc->logging_level & MPT_DEBUG_CONFIG)
		_config_display_some_debug(ioc, smid, "config_request", NULL);
	init_completion(&ioc->config_cmds.done);
	ioc->put_smid_default(ioc, smid);
	wait_for_completion_timeout(&ioc->config_cmds.done, timeout*HZ);
	if (!(ioc->config_cmds.status & MPT3_CMD_COMPLETE)) {
		if (!(ioc->logging_level & MPT_DEBUG_CONFIG))
			_config_display_some_debug(ioc,
			    smid, "config_request", NULL);
		ioc_err(ioc, "%s: command timeout\n", __func__);
		mpt3sas_base_check_cmd_timeout(ioc, ioc->config_cmds.status,
				mpi_request, sizeof(Mpi2ConfigRequest_t) / 4);
		retry_count++;
		if (ioc->config_cmds.smid == smid)
			mpt3sas_base_free_smid(ioc, smid);
		if (ioc->config_cmds.status & MPT3_CMD_RESET)
			goto retry_config;
		if (ioc->shost_recovery || ioc->pci_error_recovery) {
			issue_host_reset = 0;
			r = -EFAULT;
		} else
			issue_host_reset = 1;
		goto free_mem;
	}

	if (ioc->config_cmds.status & MPT3_CMD_REPLY_VALID) {
		memcpy(mpi_reply, ioc->config_cmds.reply,
		    sizeof(Mpi2ConfigReply_t));

		 
		if ((mpi_request->Header.PageType & 0xF) !=
		    (mpi_reply->Header.PageType & 0xF)) {
			if (!(ioc->logging_level & MPT_DEBUG_CONFIG))
				_config_display_some_debug(ioc,
				    smid, "config_request", NULL);
			_debug_dump_mf(mpi_request, ioc->request_sz/4);
			_debug_dump_reply(mpi_reply, ioc->reply_sz/4);
			panic("%s: %s: Firmware BUG: mpi_reply mismatch: Requested PageType(0x%02x) Reply PageType(0x%02x)\n",
			      ioc->name, __func__,
			      mpi_request->Header.PageType & 0xF,
			      mpi_reply->Header.PageType & 0xF);
		}

		if (((mpi_request->Header.PageType & 0xF) ==
		    MPI2_CONFIG_PAGETYPE_EXTENDED) &&
		    mpi_request->ExtPageType != mpi_reply->ExtPageType) {
			if (!(ioc->logging_level & MPT_DEBUG_CONFIG))
				_config_display_some_debug(ioc,
				    smid, "config_request", NULL);
			_debug_dump_mf(mpi_request, ioc->request_sz/4);
			_debug_dump_reply(mpi_reply, ioc->reply_sz/4);
			panic("%s: %s: Firmware BUG: mpi_reply mismatch: Requested ExtPageType(0x%02x) Reply ExtPageType(0x%02x)\n",
			      ioc->name, __func__,
			      mpi_request->ExtPageType,
			      mpi_reply->ExtPageType);
		}
		ioc_status = le16_to_cpu(mpi_reply->IOCStatus)
		    & MPI2_IOCSTATUS_MASK;
	}

	if (retry_count)
		ioc_info(ioc, "%s: retry (%d) completed!!\n",
			 __func__, retry_count);

	if ((ioc_status == MPI2_IOCSTATUS_SUCCESS) &&
	    config_page && mpi_request->Action ==
	    MPI2_CONFIG_ACTION_PAGE_READ_CURRENT) {
		u8 *p = (u8 *)mem.page;

		 
		if (p) {
			if ((mpi_request->Header.PageType & 0xF) !=
			    (p[3] & 0xF)) {
				if (!(ioc->logging_level & MPT_DEBUG_CONFIG))
					_config_display_some_debug(ioc,
					    smid, "config_request", NULL);
				_debug_dump_mf(mpi_request, ioc->request_sz/4);
				_debug_dump_reply(mpi_reply, ioc->reply_sz/4);
				_debug_dump_config(p, min_t(u16, mem.sz,
				    config_page_sz)/4);
				panic("%s: %s: Firmware BUG: config page mismatch: Requested PageType(0x%02x) Reply PageType(0x%02x)\n",
				      ioc->name, __func__,
				      mpi_request->Header.PageType & 0xF,
				      p[3] & 0xF);
			}

			if (((mpi_request->Header.PageType & 0xF) ==
			    MPI2_CONFIG_PAGETYPE_EXTENDED) &&
			    (mpi_request->ExtPageType != p[6])) {
				if (!(ioc->logging_level & MPT_DEBUG_CONFIG))
					_config_display_some_debug(ioc,
					    smid, "config_request", NULL);
				_debug_dump_mf(mpi_request, ioc->request_sz/4);
				_debug_dump_reply(mpi_reply, ioc->reply_sz/4);
				_debug_dump_config(p, min_t(u16, mem.sz,
				    config_page_sz)/4);
				panic("%s: %s: Firmware BUG: config page mismatch: Requested ExtPageType(0x%02x) Reply ExtPageType(0x%02x)\n",
				      ioc->name, __func__,
				      mpi_request->ExtPageType, p[6]);
			}
		}
		memcpy(config_page, mem.page, min_t(u16, mem.sz,
		    config_page_sz));
	}

 free_mem:
	if (config_page)
		_config_free_config_dma_memory(ioc, &mem);
 out:
	ioc->config_cmds.status = MPT3_CMD_NOT_USED;
	mutex_unlock(&ioc->config_cmds.mutex);

	if (issue_host_reset) {
		if (ioc->drv_internal_flags & MPT_DRV_INTERNAL_FIRST_PE_ISSUED) {
			mpt3sas_base_hard_reset_handler(ioc, FORCE_BIG_HAMMER);
			r = -EFAULT;
		} else {
			if (mpt3sas_base_check_for_fault_and_issue_reset(ioc))
				return -EFAULT;
			r = -EAGAIN;
		}
	}
	return r;
}

 
int
mpt3sas_config_get_manufacturing_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ManufacturingPage0_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_MANUFACTURING;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_MANUFACTURING0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_manufacturing_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ManufacturingPage1_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_MANUFACTURING;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_MANUFACTURING1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
		MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
		MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
		sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_manufacturing_pg7(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ManufacturingPage7_t *config_page,
	u16 sz)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_MANUFACTURING;
	mpi_request.Header.PageNumber = 7;
	mpi_request.Header.PageVersion = MPI2_MANUFACTURING7_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sz);
 out:
	return r;
}

 
int
mpt3sas_config_get_manufacturing_pg10(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply,
	struct Mpi2ManufacturingPage10_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_MANUFACTURING;
	mpi_request.Header.PageNumber = 10;
	mpi_request.Header.PageVersion = MPI2_MANUFACTURING0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_manufacturing_pg11(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply,
	struct Mpi2ManufacturingPage11_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_MANUFACTURING;
	mpi_request.Header.PageNumber = 11;
	mpi_request.Header.PageVersion = MPI2_MANUFACTURING0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_set_manufacturing_pg11(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply,
	struct Mpi2ManufacturingPage11_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_MANUFACTURING;
	mpi_request.Header.PageNumber = 11;
	mpi_request.Header.PageVersion = MPI2_MANUFACTURING0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_bios_pg2(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2BiosPage2_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_BIOS;
	mpi_request.Header.PageNumber = 2;
	mpi_request.Header.PageVersion = MPI2_BIOSPAGE2_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_bios_pg3(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2BiosPage3_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_BIOS;
	mpi_request.Header.PageNumber = 3;
	mpi_request.Header.PageVersion = MPI2_BIOSPAGE3_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));

 out:
	return r;
}

 
int
mpt3sas_config_set_bios_pg4(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2BiosPage4_t *config_page,
	int sz_config_pg)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));

	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_BIOS;
	mpi_request.Header.PageNumber = 4;
	mpi_request.Header.PageVersion = MPI2_BIOSPAGE4_PAGEVERSION;

	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);

	r = _config_request(ioc, &mpi_request, mpi_reply,
		MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
		MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
		sz_config_pg);
 out:
	return r;
}

 
int
mpt3sas_config_get_bios_pg4(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2BiosPage4_t *config_page,
	int sz_config_pg)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_BIOS;
	mpi_request.Header.PageNumber = 4;
	mpi_request.Header.PageVersion =  MPI2_BIOSPAGE4_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
		MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	 
	if (config_page && sz_config_pg) {
		mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;

		r = _config_request(ioc, &mpi_request, mpi_reply,
			MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
			sz_config_pg);
	}

out:
	return r;
}

 
int
mpt3sas_config_get_iounit_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOUnitPage0_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IO_UNIT;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_IOUNITPAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOUnitPage1_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IO_UNIT;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_IOUNITPAGE1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_set_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOUnitPage1_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IO_UNIT;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_IOUNITPAGE1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_iounit_pg3(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOUnitPage3_t *config_page, u16 sz)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IO_UNIT;
	mpi_request.Header.PageNumber = 3;
	mpi_request.Header.PageVersion = MPI2_IOUNITPAGE3_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page, sz);
 out:
	return r;
}

 
int
mpt3sas_config_get_iounit_pg8(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOUnitPage8_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IO_UNIT;
	mpi_request.Header.PageNumber = 8;
	mpi_request.Header.PageVersion = MPI2_IOUNITPAGE8_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_ioc_pg8(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOCPage8_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IOC;
	mpi_request.Header.PageNumber = 8;
	mpi_request.Header.PageVersion = MPI2_IOCPAGE8_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}
 
int
mpt3sas_config_get_ioc_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOCPage1_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IOC;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_IOCPAGE8_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_set_ioc_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOCPage1_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_IOC;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_IOCPAGE8_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_sas_device_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasDevicePage0_t *config_page,
	u32 form, u32 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE;
	mpi_request.Header.PageVersion = MPI2_SASDEVICE0_PAGEVERSION;
	mpi_request.Header.PageNumber = 0;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_sas_device_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasDevicePage1_t *config_page,
	u32 form, u32 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE;
	mpi_request.Header.PageVersion = MPI2_SASDEVICE1_PAGEVERSION;
	mpi_request.Header.PageNumber = 1;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_pcie_device_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26PCIeDevicePage0_t *config_page,
	u32 form, u32 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_PCIE_DEVICE;
	mpi_request.Header.PageVersion = MPI26_PCIEDEVICE0_PAGEVERSION;
	mpi_request.Header.PageNumber = 0;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
			MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
			MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
			sizeof(*config_page));
out:
	return r;
}

 
int
mpt3sas_config_get_pcie_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26PCIeIOUnitPage1_t *config_page,
	u16 sz)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_PCIE_IO_UNIT;
	mpi_request.Header.PageVersion = MPI26_PCIEIOUNITPAGE1_PAGEVERSION;
	mpi_request.Header.PageNumber = 1;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page, sz);
out:
	return r;
}

 
int
mpt3sas_config_get_pcie_device_pg2(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26PCIeDevicePage2_t *config_page,
	u32 form, u32 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_PCIE_DEVICE;
	mpi_request.Header.PageVersion = MPI26_PCIEDEVICE2_PAGEVERSION;
	mpi_request.Header.PageNumber = 2;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
			MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
			MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
			sizeof(*config_page));
out:
	return r;
}

 
int
mpt3sas_config_get_number_hba_phys(struct MPT3SAS_ADAPTER *ioc, u8 *num_phys)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;
	u16 ioc_status;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasIOUnitPage0_t config_page;

	*num_phys = 0;
	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_SASIOUNITPAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, &mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, &mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, &config_page,
	    sizeof(Mpi2SasIOUnitPage0_t));
	if (!r) {
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status == MPI2_IOCSTATUS_SUCCESS)
			*num_phys = config_page.NumPhys;
	}
 out:
	return r;
}

 
int
mpt3sas_config_get_sas_iounit_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasIOUnitPage0_t *config_page,
	u16 sz)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_SASIOUNITPAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page, sz);
 out:
	return r;
}

 
int
mpt3sas_config_get_sas_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasIOUnitPage1_t *config_page,
	u16 sz)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_SASIOUNITPAGE1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page, sz);
 out:
	return r;
}

 
int
mpt3sas_config_set_sas_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasIOUnitPage1_t *config_page,
	u16 sz)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_SASIOUNITPAGE1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	_config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page, sz);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page, sz);
 out:
	return r;
}

 
int
mpt3sas_config_get_expander_pg0(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2ExpanderPage0_t *config_page, u32 form, u32 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_SASEXPANDER0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_expander_pg1(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2ExpanderPage1_t *config_page, u32 phy_number,
	u16 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_SASEXPANDER1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress =
	    cpu_to_le32(MPI2_SAS_EXPAND_PGAD_FORM_HNDL_PHY_NUM |
	    (phy_number << MPI2_SAS_EXPAND_PGAD_PHYNUM_SHIFT) | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_enclosure_pg0(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2SasEnclosurePage0_t *config_page, u32 form, u32 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_ENCLOSURE;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_SASENCLOSURE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_phy_pg0(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2SasPhyPage0_t *config_page, u32 phy_number)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_PHY;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_SASPHY0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress =
	    cpu_to_le32(MPI2_SAS_PHY_PGAD_FORM_PHY_NUMBER | phy_number);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_phy_pg1(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2SasPhyPage1_t *config_page, u32 phy_number)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_PHY;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_SASPHY1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress =
	    cpu_to_le32(MPI2_SAS_PHY_PGAD_FORM_PHY_NUMBER | phy_number);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_raid_volume_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2RaidVolPage1_t *config_page, u32 form,
	u32 handle)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_RAID_VOLUME;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI2_RAIDVOLPAGE1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_number_pds(struct MPT3SAS_ADAPTER *ioc, u16 handle,
	u8 *num_pds)
{
	Mpi2ConfigRequest_t mpi_request;
	Mpi2RaidVolPage0_t config_page;
	Mpi2ConfigReply_t mpi_reply;
	int r;
	u16 ioc_status;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	*num_pds = 0;
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_RAID_VOLUME;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_RAIDVOLPAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, &mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress =
	    cpu_to_le32(MPI2_RAID_VOLUME_PGAD_FORM_HANDLE | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, &mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, &config_page,
	    sizeof(Mpi2RaidVolPage0_t));
	if (!r) {
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status == MPI2_IOCSTATUS_SUCCESS)
			*num_pds = config_page.NumPhysDisks;
	}

 out:
	return r;
}

 
int
mpt3sas_config_get_raid_volume_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2RaidVolPage0_t *config_page, u32 form,
	u32 handle, u16 sz)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_RAID_VOLUME;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_RAIDVOLPAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | handle);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page, sz);
 out:
	return r;
}

 
int
mpt3sas_config_get_phys_disk_pg0(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2RaidPhysDiskPage0_t *config_page, u32 form,
	u32 form_specific)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_RAID_PHYSDISK;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI2_RAIDPHYSDISKPAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.PageAddress = cpu_to_le32(form | form_specific);
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_get_driver_trigger_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage0_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
static int
_config_set_driver_trigger_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage0_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 0;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE0_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	_config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
static int
mpt3sas_config_update_driver_trigger_pg0(struct MPT3SAS_ADAPTER *ioc,
	u16 trigger_flag, bool set)
{
	Mpi26DriverTriggerPage0_t tg_pg0;
	Mpi2ConfigReply_t mpi_reply;
	int rc;
	u16 flags, ioc_status;

	rc = mpt3sas_config_get_driver_trigger_pg0(ioc, &mpi_reply, &tg_pg0);
	if (rc)
		return rc;
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg0, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		return -EFAULT;
	}

	if (set)
		flags = le16_to_cpu(tg_pg0.TriggerFlags) | trigger_flag;
	else
		flags = le16_to_cpu(tg_pg0.TriggerFlags) & ~trigger_flag;

	tg_pg0.TriggerFlags = cpu_to_le16(flags);

	rc = _config_set_driver_trigger_pg0(ioc, &mpi_reply, &tg_pg0);
	if (rc)
		return rc;
	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to update trigger pg0, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		return -EFAULT;
	}

	return 0;
}

 
int
mpt3sas_config_get_driver_trigger_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage1_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
static int
_config_set_driver_trigger_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage1_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 1;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE1_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	_config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_update_driver_trigger_pg1(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_MASTER_TRIGGER_T *master_tg, bool set)
{
	Mpi26DriverTriggerPage1_t tg_pg1;
	Mpi2ConfigReply_t mpi_reply;
	int rc;
	u16 ioc_status;

	rc = mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_MASTER_TRIGGER_VALID, set);
	if (rc)
		return rc;

	rc = mpt3sas_config_get_driver_trigger_pg1(ioc, &mpi_reply, &tg_pg1);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg1, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		rc = -EFAULT;
		goto out;
	}

	if (set) {
		tg_pg1.NumMasterTrigger = cpu_to_le16(1);
		tg_pg1.MasterTriggers[0].MasterTriggerFlags = cpu_to_le32(
		    master_tg->MasterData);
	} else {
		tg_pg1.NumMasterTrigger = 0;
		tg_pg1.MasterTriggers[0].MasterTriggerFlags = 0;
	}

	rc = _config_set_driver_trigger_pg1(ioc, &mpi_reply, &tg_pg1);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg1, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		rc = -EFAULT;
		goto out;
	}

	return 0;

out:
	mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_MASTER_TRIGGER_VALID, !set);

	return rc;
}

 
int
mpt3sas_config_get_driver_trigger_pg2(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage2_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 2;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE2_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
static int
_config_set_driver_trigger_pg2(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage2_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 2;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE2_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	_config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_update_driver_trigger_pg2(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_EVENT_TRIGGERS_T *event_tg, bool set)
{
	Mpi26DriverTriggerPage2_t tg_pg2;
	Mpi2ConfigReply_t mpi_reply;
	int rc, i, count;
	u16 ioc_status;

	rc = mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_MPI_EVENT_TRIGGER_VALID, set);
	if (rc)
		return rc;

	rc = mpt3sas_config_get_driver_trigger_pg2(ioc, &mpi_reply, &tg_pg2);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg2, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		rc = -EFAULT;
		goto out;
	}

	if (set) {
		count = event_tg->ValidEntries;
		tg_pg2.NumMPIEventTrigger = cpu_to_le16(count);
		for (i = 0; i < count; i++) {
			tg_pg2.MPIEventTriggers[i].MPIEventCode =
			    cpu_to_le16(
			    event_tg->EventTriggerEntry[i].EventValue);
			tg_pg2.MPIEventTriggers[i].MPIEventCodeSpecific =
			    cpu_to_le16(
			    event_tg->EventTriggerEntry[i].LogEntryQualifier);
		}
	} else {
		tg_pg2.NumMPIEventTrigger = 0;
		memset(&tg_pg2.MPIEventTriggers[0], 0,
		    NUM_VALID_ENTRIES * sizeof(
		    MPI26_DRIVER_MPI_EVENT_TIGGER_ENTRY));
	}

	rc = _config_set_driver_trigger_pg2(ioc, &mpi_reply, &tg_pg2);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg2, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		rc = -EFAULT;
		goto out;
	}

	return 0;

out:
	mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_MPI_EVENT_TRIGGER_VALID, !set);

	return rc;
}

 
int
mpt3sas_config_get_driver_trigger_pg3(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage3_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 3;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE3_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
static int
_config_set_driver_trigger_pg3(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage3_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 3;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE3_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	_config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_update_driver_trigger_pg3(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_SCSI_TRIGGERS_T *scsi_tg, bool set)
{
	Mpi26DriverTriggerPage3_t tg_pg3;
	Mpi2ConfigReply_t mpi_reply;
	int rc, i, count;
	u16 ioc_status;

	rc = mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_SCSI_SENSE_TRIGGER_VALID, set);
	if (rc)
		return rc;

	rc = mpt3sas_config_get_driver_trigger_pg3(ioc, &mpi_reply, &tg_pg3);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg3, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		return -EFAULT;
	}

	if (set) {
		count = scsi_tg->ValidEntries;
		tg_pg3.NumSCSISenseTrigger = cpu_to_le16(count);
		for (i = 0; i < count; i++) {
			tg_pg3.SCSISenseTriggers[i].ASCQ =
			    scsi_tg->SCSITriggerEntry[i].ASCQ;
			tg_pg3.SCSISenseTriggers[i].ASC =
			    scsi_tg->SCSITriggerEntry[i].ASC;
			tg_pg3.SCSISenseTriggers[i].SenseKey =
			    scsi_tg->SCSITriggerEntry[i].SenseKey;
		}
	} else {
		tg_pg3.NumSCSISenseTrigger = 0;
		memset(&tg_pg3.SCSISenseTriggers[0], 0,
		    NUM_VALID_ENTRIES * sizeof(
		    MPI26_DRIVER_SCSI_SENSE_TIGGER_ENTRY));
	}

	rc = _config_set_driver_trigger_pg3(ioc, &mpi_reply, &tg_pg3);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg3, ioc_status(0x%04x)\n",
		     __func__, ioc_status));
		return -EFAULT;
	}

	return 0;
out:
	mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_SCSI_SENSE_TRIGGER_VALID, !set);

	return rc;
}

 
int
mpt3sas_config_get_driver_trigger_pg4(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage4_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 4;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE4_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
static int
_config_set_driver_trigger_pg4(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage4_t *config_page)
{
	Mpi2ConfigRequest_t mpi_request;
	int r;

	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType =
	    MPI2_CONFIG_EXTPAGETYPE_DRIVER_PERSISTENT_TRIGGER;
	mpi_request.Header.PageNumber = 4;
	mpi_request.Header.PageVersion = MPI26_DRIVER_TRIGGER_PAGE4_PAGEVERSION;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	_config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM;
	r = _config_request(ioc, &mpi_request, mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
	    sizeof(*config_page));
 out:
	return r;
}

 
int
mpt3sas_config_update_driver_trigger_pg4(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_MPI_TRIGGERS_T *mpi_tg, bool set)
{
	Mpi26DriverTriggerPage4_t tg_pg4;
	Mpi2ConfigReply_t mpi_reply;
	int rc, i, count;
	u16 ioc_status;

	rc = mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_LOGINFO_TRIGGER_VALID, set);
	if (rc)
		return rc;

	rc = mpt3sas_config_get_driver_trigger_pg4(ioc, &mpi_reply, &tg_pg4);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg4, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		rc = -EFAULT;
		goto out;
	}

	if (set) {
		count = mpi_tg->ValidEntries;
		tg_pg4.NumIOCStatusLogInfoTrigger = cpu_to_le16(count);
		for (i = 0; i < count; i++) {
			tg_pg4.IOCStatusLoginfoTriggers[i].IOCStatus =
			    cpu_to_le16(mpi_tg->MPITriggerEntry[i].IOCStatus);
			tg_pg4.IOCStatusLoginfoTriggers[i].LogInfo =
			    cpu_to_le32(mpi_tg->MPITriggerEntry[i].IocLogInfo);
		}
	} else {
		tg_pg4.NumIOCStatusLogInfoTrigger = 0;
		memset(&tg_pg4.IOCStatusLoginfoTriggers[0], 0,
		    NUM_VALID_ENTRIES * sizeof(
		    MPI26_DRIVER_IOCSTATUS_LOGINFO_TIGGER_ENTRY));
	}

	rc = _config_set_driver_trigger_pg4(ioc, &mpi_reply, &tg_pg4);
	if (rc)
		goto out;

	ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
	    MPI2_IOCSTATUS_MASK;
	if (ioc_status != MPI2_IOCSTATUS_SUCCESS) {
		dcprintk(ioc,
		    ioc_err(ioc,
		    "%s: Failed to get trigger pg4, ioc_status(0x%04x)\n",
		    __func__, ioc_status));
		rc = -EFAULT;
		goto out;
	}

	return 0;

out:
	mpt3sas_config_update_driver_trigger_pg0(ioc,
	    MPI26_DRIVER_TRIGGER0_FLAG_LOGINFO_TRIGGER_VALID, !set);

	return rc;
}

 
int
mpt3sas_config_get_volume_handle(struct MPT3SAS_ADAPTER *ioc, u16 pd_handle,
	u16 *volume_handle)
{
	Mpi2RaidConfigurationPage0_t *config_page = NULL;
	Mpi2ConfigRequest_t mpi_request;
	Mpi2ConfigReply_t mpi_reply;
	int r, i, config_page_sz;
	u16 ioc_status;
	int config_num;
	u16 element_type;
	u16 phys_disk_dev_handle;

	*volume_handle = 0;
	memset(&mpi_request, 0, sizeof(Mpi2ConfigRequest_t));
	mpi_request.Function = MPI2_FUNCTION_CONFIG;
	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	mpi_request.Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	mpi_request.ExtPageType = MPI2_CONFIG_EXTPAGETYPE_RAID_CONFIG;
	mpi_request.Header.PageVersion = MPI2_RAIDCONFIG0_PAGEVERSION;
	mpi_request.Header.PageNumber = 0;
	ioc->build_zero_len_sge_mpi(ioc, &mpi_request.PageBufferSGE);
	r = _config_request(ioc, &mpi_request, &mpi_reply,
	    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, NULL, 0);
	if (r)
		goto out;

	mpi_request.Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	config_page_sz = (le16_to_cpu(mpi_reply.ExtPageLength) * 4);
	config_page = kmalloc(config_page_sz, GFP_KERNEL);
	if (!config_page) {
		r = -1;
		goto out;
	}

	config_num = 0xff;
	while (1) {
		mpi_request.PageAddress = cpu_to_le32(config_num +
		    MPI2_RAID_PGAD_FORM_GET_NEXT_CONFIGNUM);
		r = _config_request(ioc, &mpi_request, &mpi_reply,
		    MPT3_CONFIG_PAGE_DEFAULT_TIMEOUT, config_page,
		    config_page_sz);
		if (r)
			goto out;
		r = -1;
		ioc_status = le16_to_cpu(mpi_reply.IOCStatus) &
		    MPI2_IOCSTATUS_MASK;
		if (ioc_status != MPI2_IOCSTATUS_SUCCESS)
			goto out;
		for (i = 0; i < config_page->NumElements; i++) {
			element_type = le16_to_cpu(config_page->
			    ConfigElement[i].ElementFlags) &
			    MPI2_RAIDCONFIG0_EFLAGS_MASK_ELEMENT_TYPE;
			if (element_type ==
			    MPI2_RAIDCONFIG0_EFLAGS_VOL_PHYS_DISK_ELEMENT ||
			    element_type ==
			    MPI2_RAIDCONFIG0_EFLAGS_OCE_ELEMENT) {
				phys_disk_dev_handle =
				    le16_to_cpu(config_page->ConfigElement[i].
				    PhysDiskDevHandle);
				if (phys_disk_dev_handle == pd_handle) {
					*volume_handle =
					    le16_to_cpu(config_page->
					    ConfigElement[i].VolDevHandle);
					r = 0;
					goto out;
				}
			} else if (element_type ==
			    MPI2_RAIDCONFIG0_EFLAGS_HOT_SPARE_ELEMENT) {
				*volume_handle = 0;
				r = 0;
				goto out;
			}
		}
		config_num = config_page->ConfigNum;
	}
 out:
	kfree(config_page);
	return r;
}

 
int
mpt3sas_config_get_volume_wwid(struct MPT3SAS_ADAPTER *ioc, u16 volume_handle,
	u64 *wwid)
{
	Mpi2ConfigReply_t mpi_reply;
	Mpi2RaidVolPage1_t raid_vol_pg1;

	*wwid = 0;
	if (!(mpt3sas_config_get_raid_volume_pg1(ioc, &mpi_reply,
	    &raid_vol_pg1, MPI2_RAID_VOLUME_PGAD_FORM_HANDLE,
	    volume_handle))) {
		*wwid = le64_to_cpu(raid_vol_pg1.WWID);
		return 0;
	} else
		return -1;
}
