
 

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/intel-ish-client-if.h>
#include <linux/property.h>
#include <asm/cacheflush.h>

 
#define MAX_LOAD_ATTEMPTS			3

 
#define LOADER_CL_RX_RING_SIZE			1
#define LOADER_CL_TX_RING_SIZE			1

 
#define LOADER_SHIM_IPC_BUF_SIZE		3968

 
enum ish_loader_commands {
	LOADER_CMD_XFER_QUERY = 0,
	LOADER_CMD_XFER_FRAGMENT,
	LOADER_CMD_START,
};

 
#define	CMD_MASK				GENMASK(6, 0)
#define	IS_RESPONSE				BIT(7)

 
#define ISHTP_SEND_TIMEOUT			(3 * HZ)

 
#define LOADER_XFER_MODE_DIRECT_DMA		BIT(0)
#define LOADER_XFER_MODE_ISHTP			BIT(1)

 
static const struct ishtp_device_id loader_ishtp_id_table[] = {
	{ .guid = GUID_INIT(0xc804d06a, 0x55bd, 0x4ea7,
		  0xad, 0xed, 0x1e, 0x31, 0x22, 0x8c, 0x76, 0xdc) },
	{ }
};
MODULE_DEVICE_TABLE(ishtp, loader_ishtp_id_table);

#define FILENAME_SIZE				256

 
static int dma_buf_size_limit = 4 * PAGE_SIZE;

 
struct loader_msg_hdr {
	u8 command;
	u8 reserved[2];
	u8 status;
} __packed;

struct loader_xfer_query {
	struct loader_msg_hdr hdr;
	u32 image_size;
} __packed;

struct ish_fw_version {
	u16 major;
	u16 minor;
	u16 hotfix;
	u16 build;
} __packed;

union loader_version {
	u32 value;
	struct {
		u8 major;
		u8 minor;
		u8 hotfix;
		u8 build;
	};
} __packed;

struct loader_capability {
	u32 max_fw_image_size;
	u32 xfer_mode;
	u32 max_dma_buf_size;  
} __packed;

struct shim_fw_info {
	struct ish_fw_version ish_fw_version;
	u32 protocol_version;
	union loader_version ldr_version;
	struct loader_capability ldr_capability;
} __packed;

struct loader_xfer_query_response {
	struct loader_msg_hdr hdr;
	struct shim_fw_info fw_info;
} __packed;

struct loader_xfer_fragment {
	struct loader_msg_hdr hdr;
	u32 xfer_mode;
	u32 offset;
	u32 size;
	u32 is_last;
} __packed;

struct loader_xfer_ipc_fragment {
	struct loader_xfer_fragment fragment;
	u8 data[] ____cacheline_aligned;  
} __packed;

struct loader_xfer_dma_fragment {
	struct loader_xfer_fragment fragment;
	u64 ddr_phys_addr;
} __packed;

struct loader_start {
	struct loader_msg_hdr hdr;
} __packed;

 
struct response_info {
	void *data;
	size_t max_size;
	size_t size;
	int error;
	bool received;
	wait_queue_head_t wait_queue;
};

 
struct ishtp_cl_data {
	struct ishtp_cl *loader_ishtp_cl;
	struct ishtp_cl_device *cl_device;

	 
	struct response_info response;

	struct work_struct work_ishtp_reset;
	struct work_struct work_fw_load;

	 
	bool flag_retry;
	int retry_count;
};

#define IPC_FRAGMENT_DATA_PREAMBLE				\
	offsetof(struct loader_xfer_ipc_fragment, data)

#define cl_data_to_dev(client_data) ishtp_device((client_data)->cl_device)

 
static int get_firmware_variant(struct ishtp_cl_data *client_data,
				char *filename)
{
	int rv;
	const char *val;
	struct device *devc = ishtp_get_pci_device(client_data->cl_device);

	rv = device_property_read_string(devc, "firmware-name", &val);
	if (rv < 0) {
		dev_err(devc,
			"Error: ISH firmware-name device property required\n");
		return rv;
	}
	return snprintf(filename, FILENAME_SIZE, "intel/%s", val);
}

 
static int loader_cl_send(struct ishtp_cl_data *client_data,
			  u8 *out_msg, size_t out_size,
			  u8 *in_msg, size_t in_size)
{
	int rv;
	struct loader_msg_hdr *out_hdr = (struct loader_msg_hdr *)out_msg;
	struct ishtp_cl *loader_ishtp_cl = client_data->loader_ishtp_cl;

	dev_dbg(cl_data_to_dev(client_data),
		"%s: command=%02lx is_response=%u status=%02x\n",
		__func__,
		out_hdr->command & CMD_MASK,
		out_hdr->command & IS_RESPONSE ? 1 : 0,
		out_hdr->status);

	 
	client_data->response.data = in_msg;
	client_data->response.max_size = in_size;
	client_data->response.error = 0;
	client_data->response.received = false;

	rv = ishtp_cl_send(loader_ishtp_cl, out_msg, out_size);
	if (rv < 0) {
		dev_err(cl_data_to_dev(client_data),
			"ishtp_cl_send error %d\n", rv);
		return rv;
	}

	wait_event_interruptible_timeout(client_data->response.wait_queue,
					 client_data->response.received,
					 ISHTP_SEND_TIMEOUT);
	if (!client_data->response.received) {
		dev_err(cl_data_to_dev(client_data),
			"Timed out for response to command=%02lx",
			out_hdr->command & CMD_MASK);
		return -ETIMEDOUT;
	}

	if (client_data->response.error < 0)
		return client_data->response.error;

	return client_data->response.size;
}

 
static void process_recv(struct ishtp_cl *loader_ishtp_cl,
			 struct ishtp_cl_rb *rb_in_proc)
{
	struct loader_msg_hdr *hdr;
	size_t data_len = rb_in_proc->buf_idx;
	struct ishtp_cl_data *client_data =
		ishtp_get_client_data(loader_ishtp_cl);

	 
	if (!client_data->response.data) {
		dev_err(cl_data_to_dev(client_data),
			"Receiving buffer is null. Should be allocated by calling function\n");
		client_data->response.error = -EINVAL;
		goto end;
	}

	if (client_data->response.received) {
		dev_err(cl_data_to_dev(client_data),
			"Previous firmware message not yet processed\n");
		client_data->response.error = -EINVAL;
		goto end;
	}
	 
	if (!rb_in_proc->buffer.data) {
		dev_warn(cl_data_to_dev(client_data),
			 "rb_in_proc->buffer.data returned null");
		client_data->response.error = -EBADMSG;
		goto end;
	}

	if (data_len < sizeof(struct loader_msg_hdr)) {
		dev_err(cl_data_to_dev(client_data),
			"data size %zu is less than header %zu\n",
			data_len, sizeof(struct loader_msg_hdr));
		client_data->response.error = -EMSGSIZE;
		goto end;
	}

	hdr = (struct loader_msg_hdr *)rb_in_proc->buffer.data;

	dev_dbg(cl_data_to_dev(client_data),
		"%s: command=%02lx is_response=%u status=%02x\n",
		__func__,
		hdr->command & CMD_MASK,
		hdr->command & IS_RESPONSE ? 1 : 0,
		hdr->status);

	if (((hdr->command & CMD_MASK) != LOADER_CMD_XFER_QUERY) &&
	    ((hdr->command & CMD_MASK) != LOADER_CMD_XFER_FRAGMENT) &&
	    ((hdr->command & CMD_MASK) != LOADER_CMD_START)) {
		dev_err(cl_data_to_dev(client_data),
			"Invalid command=%02lx\n",
			hdr->command & CMD_MASK);
		client_data->response.error = -EPROTO;
		goto end;
	}

	if (data_len > client_data->response.max_size) {
		dev_err(cl_data_to_dev(client_data),
			"Received buffer size %zu is larger than allocated buffer %zu\n",
			data_len, client_data->response.max_size);
		client_data->response.error = -EMSGSIZE;
		goto end;
	}

	 
	if (!(hdr->command & IS_RESPONSE)) {
		dev_err(cl_data_to_dev(client_data),
			"Invalid response to command\n");
		client_data->response.error = -EIO;
		goto end;
	}

	if (hdr->status) {
		dev_err(cl_data_to_dev(client_data),
			"Loader returned status %d\n",
			hdr->status);
		client_data->response.error = -EIO;
		goto end;
	}

	 
	client_data->response.size = data_len;

	 
	memcpy(client_data->response.data,
	       rb_in_proc->buffer.data, data_len);

	 
	client_data->response.received = true;

end:
	 
	ishtp_cl_io_rb_recycle(rb_in_proc);
	rb_in_proc = NULL;

	 
	wake_up_interruptible(&client_data->response.wait_queue);
}

 
static void loader_cl_event_cb(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_rb *rb_in_proc;
	struct ishtp_cl	*loader_ishtp_cl = ishtp_get_drvdata(cl_device);

	while ((rb_in_proc = ishtp_cl_rx_get_rb(loader_ishtp_cl)) != NULL) {
		 
		process_recv(loader_ishtp_cl, rb_in_proc);
	}
}

 
static int ish_query_loader_prop(struct ishtp_cl_data *client_data,
				 const struct firmware *fw,
				 struct shim_fw_info *fw_info)
{
	int rv;
	struct loader_xfer_query ldr_xfer_query;
	struct loader_xfer_query_response ldr_xfer_query_resp;

	memset(&ldr_xfer_query, 0, sizeof(ldr_xfer_query));
	ldr_xfer_query.hdr.command = LOADER_CMD_XFER_QUERY;
	ldr_xfer_query.image_size = fw->size;
	rv = loader_cl_send(client_data,
			    (u8 *)&ldr_xfer_query,
			    sizeof(ldr_xfer_query),
			    (u8 *)&ldr_xfer_query_resp,
			    sizeof(ldr_xfer_query_resp));
	if (rv < 0) {
		client_data->flag_retry = true;
		*fw_info = (struct shim_fw_info){};
		return rv;
	}

	 
	if (rv != sizeof(struct loader_xfer_query_response)) {
		dev_err(cl_data_to_dev(client_data),
			"data size %d is not equal to size of loader_xfer_query_response %zu\n",
			rv, sizeof(struct loader_xfer_query_response));
		client_data->flag_retry = true;
		*fw_info = (struct shim_fw_info){};
		return -EMSGSIZE;
	}

	 
	*fw_info = ldr_xfer_query_resp.fw_info;

	 
	dev_dbg(cl_data_to_dev(client_data),
		"ish_fw_version: major=%d minor=%d hotfix=%d build=%d protocol_version=0x%x loader_version=%d\n",
		fw_info->ish_fw_version.major,
		fw_info->ish_fw_version.minor,
		fw_info->ish_fw_version.hotfix,
		fw_info->ish_fw_version.build,
		fw_info->protocol_version,
		fw_info->ldr_version.value);

	dev_dbg(cl_data_to_dev(client_data),
		"loader_capability: max_fw_image_size=0x%x xfer_mode=%d max_dma_buf_size=0x%x dma_buf_size_limit=0x%x\n",
		fw_info->ldr_capability.max_fw_image_size,
		fw_info->ldr_capability.xfer_mode,
		fw_info->ldr_capability.max_dma_buf_size,
		dma_buf_size_limit);

	 
	if (fw_info->ldr_capability.max_fw_image_size < fw->size) {
		dev_err(cl_data_to_dev(client_data),
			"ISH firmware size %zu is greater than Shim firmware loader max supported %d\n",
			fw->size,
			fw_info->ldr_capability.max_fw_image_size);
		return -ENOSPC;
	}

	 
	if ((fw_info->ldr_capability.xfer_mode & LOADER_XFER_MODE_DIRECT_DMA) &&
	    (fw_info->ldr_capability.max_dma_buf_size % L1_CACHE_BYTES)) {
		dev_err(cl_data_to_dev(client_data),
			"Shim firmware loader buffer size %d should be multiple of cacheline\n",
			fw_info->ldr_capability.max_dma_buf_size);
		return -EINVAL;
	}

	return 0;
}

 
static int ish_fw_xfer_ishtp(struct ishtp_cl_data *client_data,
			     const struct firmware *fw)
{
	int rv;
	u32 fragment_offset, fragment_size, payload_max_size;
	struct loader_xfer_ipc_fragment *ldr_xfer_ipc_frag;
	struct loader_msg_hdr ldr_xfer_ipc_ack;

	payload_max_size =
		LOADER_SHIM_IPC_BUF_SIZE - IPC_FRAGMENT_DATA_PREAMBLE;

	ldr_xfer_ipc_frag = kzalloc(LOADER_SHIM_IPC_BUF_SIZE, GFP_KERNEL);
	if (!ldr_xfer_ipc_frag) {
		client_data->flag_retry = true;
		return -ENOMEM;
	}

	ldr_xfer_ipc_frag->fragment.hdr.command = LOADER_CMD_XFER_FRAGMENT;
	ldr_xfer_ipc_frag->fragment.xfer_mode = LOADER_XFER_MODE_ISHTP;

	 
	fragment_offset = 0;
	while (fragment_offset < fw->size) {
		if (fragment_offset + payload_max_size < fw->size) {
			fragment_size = payload_max_size;
			ldr_xfer_ipc_frag->fragment.is_last = 0;
		} else {
			fragment_size = fw->size - fragment_offset;
			ldr_xfer_ipc_frag->fragment.is_last = 1;
		}

		ldr_xfer_ipc_frag->fragment.offset = fragment_offset;
		ldr_xfer_ipc_frag->fragment.size = fragment_size;
		memcpy(ldr_xfer_ipc_frag->data,
		       &fw->data[fragment_offset],
		       fragment_size);

		dev_dbg(cl_data_to_dev(client_data),
			"xfer_mode=ipc offset=0x%08x size=0x%08x is_last=%d\n",
			ldr_xfer_ipc_frag->fragment.offset,
			ldr_xfer_ipc_frag->fragment.size,
			ldr_xfer_ipc_frag->fragment.is_last);

		rv = loader_cl_send(client_data,
				    (u8 *)ldr_xfer_ipc_frag,
				    IPC_FRAGMENT_DATA_PREAMBLE + fragment_size,
				    (u8 *)&ldr_xfer_ipc_ack,
				    sizeof(ldr_xfer_ipc_ack));
		if (rv < 0) {
			client_data->flag_retry = true;
			goto end_err_resp_buf_release;
		}

		fragment_offset += fragment_size;
	}

	kfree(ldr_xfer_ipc_frag);
	return 0;

end_err_resp_buf_release:
	 
	kfree(ldr_xfer_ipc_frag);
	return rv;
}

 
static int ish_fw_xfer_direct_dma(struct ishtp_cl_data *client_data,
				  const struct firmware *fw,
				  const struct shim_fw_info fw_info)
{
	int rv;
	void *dma_buf;
	dma_addr_t dma_buf_phy;
	u32 fragment_offset, fragment_size, payload_max_size;
	struct loader_msg_hdr ldr_xfer_dma_frag_ack;
	struct loader_xfer_dma_fragment ldr_xfer_dma_frag;
	struct device *devc = ishtp_get_pci_device(client_data->cl_device);
	u32 shim_fw_buf_size =
		fw_info.ldr_capability.max_dma_buf_size;

	 
	payload_max_size = min3(fw->size,
				(size_t)shim_fw_buf_size,
				(size_t)dma_buf_size_limit);

	 
	payload_max_size &= ~(L1_CACHE_BYTES - 1);

	dma_buf = dma_alloc_coherent(devc, payload_max_size, &dma_buf_phy, GFP_KERNEL);
	if (!dma_buf) {
		client_data->flag_retry = true;
		return -ENOMEM;
	}

	ldr_xfer_dma_frag.fragment.hdr.command = LOADER_CMD_XFER_FRAGMENT;
	ldr_xfer_dma_frag.fragment.xfer_mode = LOADER_XFER_MODE_DIRECT_DMA;
	ldr_xfer_dma_frag.ddr_phys_addr = (u64)dma_buf_phy;

	 
	fragment_offset = 0;
	while (fragment_offset < fw->size) {
		if (fragment_offset + payload_max_size < fw->size) {
			fragment_size = payload_max_size;
			ldr_xfer_dma_frag.fragment.is_last = 0;
		} else {
			fragment_size = fw->size - fragment_offset;
			ldr_xfer_dma_frag.fragment.is_last = 1;
		}

		ldr_xfer_dma_frag.fragment.offset = fragment_offset;
		ldr_xfer_dma_frag.fragment.size = fragment_size;
		memcpy(dma_buf, &fw->data[fragment_offset], fragment_size);

		 
		clflush_cache_range(dma_buf, payload_max_size);

		dev_dbg(cl_data_to_dev(client_data),
			"xfer_mode=dma offset=0x%08x size=0x%x is_last=%d ddr_phys_addr=0x%016llx\n",
			ldr_xfer_dma_frag.fragment.offset,
			ldr_xfer_dma_frag.fragment.size,
			ldr_xfer_dma_frag.fragment.is_last,
			ldr_xfer_dma_frag.ddr_phys_addr);

		rv = loader_cl_send(client_data,
				    (u8 *)&ldr_xfer_dma_frag,
				    sizeof(ldr_xfer_dma_frag),
				    (u8 *)&ldr_xfer_dma_frag_ack,
				    sizeof(ldr_xfer_dma_frag_ack));
		if (rv < 0) {
			client_data->flag_retry = true;
			goto end_err_resp_buf_release;
		}

		fragment_offset += fragment_size;
	}

end_err_resp_buf_release:
	dma_free_coherent(devc, payload_max_size, dma_buf, dma_buf_phy);
	return rv;
}

 
static int ish_fw_start(struct ishtp_cl_data *client_data)
{
	struct loader_start ldr_start;
	struct loader_msg_hdr ldr_start_ack;

	memset(&ldr_start, 0, sizeof(ldr_start));
	ldr_start.hdr.command = LOADER_CMD_START;
	return loader_cl_send(client_data,
			    (u8 *)&ldr_start,
			    sizeof(ldr_start),
			    (u8 *)&ldr_start_ack,
			    sizeof(ldr_start_ack));
}

 
static int load_fw_from_host(struct ishtp_cl_data *client_data)
{
	int rv;
	u32 xfer_mode;
	char *filename;
	const struct firmware *fw;
	struct shim_fw_info fw_info;
	struct ishtp_cl *loader_ishtp_cl = client_data->loader_ishtp_cl;

	client_data->flag_retry = false;

	filename = kzalloc(FILENAME_SIZE, GFP_KERNEL);
	if (!filename) {
		client_data->flag_retry = true;
		rv = -ENOMEM;
		goto end_error;
	}

	 
	rv = get_firmware_variant(client_data, filename);
	if (rv < 0)
		goto end_err_filename_buf_release;

	rv = request_firmware(&fw, filename, cl_data_to_dev(client_data));
	if (rv < 0)
		goto end_err_filename_buf_release;

	 

	rv = ish_query_loader_prop(client_data, fw, &fw_info);
	if (rv < 0)
		goto end_err_fw_release;

	 

	xfer_mode = fw_info.ldr_capability.xfer_mode;
	if (xfer_mode & LOADER_XFER_MODE_DIRECT_DMA) {
		rv = ish_fw_xfer_direct_dma(client_data, fw, fw_info);
	} else if (xfer_mode & LOADER_XFER_MODE_ISHTP) {
		rv = ish_fw_xfer_ishtp(client_data, fw);
	} else {
		dev_err(cl_data_to_dev(client_data),
			"No transfer mode selected in firmware\n");
		rv = -EINVAL;
	}
	if (rv < 0)
		goto end_err_fw_release;

	 

	rv = ish_fw_start(client_data);
	if (rv < 0)
		goto end_err_fw_release;

	release_firmware(fw);
	dev_info(cl_data_to_dev(client_data), "ISH firmware %s loaded\n",
		 filename);
	kfree(filename);
	return 0;

end_err_fw_release:
	release_firmware(fw);
end_err_filename_buf_release:
	kfree(filename);
end_error:
	 
	if (client_data->flag_retry &&
	    client_data->retry_count++ < MAX_LOAD_ATTEMPTS) {
		dev_warn(cl_data_to_dev(client_data),
			 "ISH host firmware load failed %d. Resetting ISH, and trying again..\n",
			 rv);
		ish_hw_reset(ishtp_get_ishtp_device(loader_ishtp_cl));
	} else {
		dev_err(cl_data_to_dev(client_data),
			"ISH host firmware load failed %d\n", rv);
	}
	return rv;
}

static void load_fw_from_host_handler(struct work_struct *work)
{
	struct ishtp_cl_data *client_data;

	client_data = container_of(work, struct ishtp_cl_data,
				   work_fw_load);
	load_fw_from_host(client_data);
}

 
static int loader_init(struct ishtp_cl *loader_ishtp_cl, int reset)
{
	int rv;
	struct ishtp_fw_client *fw_client;
	struct ishtp_cl_data *client_data =
		ishtp_get_client_data(loader_ishtp_cl);

	dev_dbg(cl_data_to_dev(client_data), "reset flag: %d\n", reset);

	rv = ishtp_cl_link(loader_ishtp_cl);
	if (rv < 0) {
		dev_err(cl_data_to_dev(client_data), "ishtp_cl_link failed\n");
		return rv;
	}

	 
	ishtp_set_tx_ring_size(loader_ishtp_cl, LOADER_CL_TX_RING_SIZE);
	ishtp_set_rx_ring_size(loader_ishtp_cl, LOADER_CL_RX_RING_SIZE);

	fw_client =
		ishtp_fw_cl_get_client(ishtp_get_ishtp_device(loader_ishtp_cl),
				       &loader_ishtp_id_table[0].guid);
	if (!fw_client) {
		dev_err(cl_data_to_dev(client_data),
			"ISH client uuid not found\n");
		rv = -ENOENT;
		goto err_cl_unlink;
	}

	ishtp_cl_set_fw_client_id(loader_ishtp_cl,
				  ishtp_get_fw_client_id(fw_client));
	ishtp_set_connection_state(loader_ishtp_cl, ISHTP_CL_CONNECTING);

	rv = ishtp_cl_connect(loader_ishtp_cl);
	if (rv < 0) {
		dev_err(cl_data_to_dev(client_data), "Client connect fail\n");
		goto err_cl_unlink;
	}

	dev_dbg(cl_data_to_dev(client_data), "Client connected\n");

	ishtp_register_event_cb(client_data->cl_device, loader_cl_event_cb);

	return 0;

err_cl_unlink:
	ishtp_cl_unlink(loader_ishtp_cl);
	return rv;
}

static void loader_deinit(struct ishtp_cl *loader_ishtp_cl)
{
	ishtp_set_connection_state(loader_ishtp_cl, ISHTP_CL_DISCONNECTING);
	ishtp_cl_disconnect(loader_ishtp_cl);
	ishtp_cl_unlink(loader_ishtp_cl);
	ishtp_cl_flush_queues(loader_ishtp_cl);

	 
	ishtp_cl_free(loader_ishtp_cl);
}

static void reset_handler(struct work_struct *work)
{
	int rv;
	struct ishtp_cl_data *client_data;
	struct ishtp_cl *loader_ishtp_cl;
	struct ishtp_cl_device *cl_device;

	client_data = container_of(work, struct ishtp_cl_data,
				   work_ishtp_reset);

	loader_ishtp_cl = client_data->loader_ishtp_cl;
	cl_device = client_data->cl_device;

	 
	ishtp_cl_unlink(loader_ishtp_cl);
	ishtp_cl_flush_queues(loader_ishtp_cl);
	ishtp_cl_free(loader_ishtp_cl);

	loader_ishtp_cl = ishtp_cl_allocate(cl_device);
	if (!loader_ishtp_cl)
		return;

	ishtp_set_drvdata(cl_device, loader_ishtp_cl);
	ishtp_set_client_data(loader_ishtp_cl, client_data);
	client_data->loader_ishtp_cl = loader_ishtp_cl;
	client_data->cl_device = cl_device;

	rv = loader_init(loader_ishtp_cl, 1);
	if (rv < 0) {
		dev_err(ishtp_device(cl_device), "Reset Failed\n");
		return;
	}

	 
	load_fw_from_host(client_data);
}

 
static int loader_ishtp_cl_probe(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl *loader_ishtp_cl;
	struct ishtp_cl_data *client_data;
	int rv;

	client_data = devm_kzalloc(ishtp_device(cl_device),
				   sizeof(*client_data),
				   GFP_KERNEL);
	if (!client_data)
		return -ENOMEM;

	loader_ishtp_cl = ishtp_cl_allocate(cl_device);
	if (!loader_ishtp_cl)
		return -ENOMEM;

	ishtp_set_drvdata(cl_device, loader_ishtp_cl);
	ishtp_set_client_data(loader_ishtp_cl, client_data);
	client_data->loader_ishtp_cl = loader_ishtp_cl;
	client_data->cl_device = cl_device;

	init_waitqueue_head(&client_data->response.wait_queue);

	INIT_WORK(&client_data->work_ishtp_reset,
		  reset_handler);
	INIT_WORK(&client_data->work_fw_load,
		  load_fw_from_host_handler);

	rv = loader_init(loader_ishtp_cl, 0);
	if (rv < 0) {
		ishtp_cl_free(loader_ishtp_cl);
		return rv;
	}
	ishtp_get_device(cl_device);

	client_data->retry_count = 0;

	 
	schedule_work(&client_data->work_fw_load);

	return 0;
}

 
static void loader_ishtp_cl_remove(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_data *client_data;
	struct ishtp_cl	*loader_ishtp_cl = ishtp_get_drvdata(cl_device);

	client_data = ishtp_get_client_data(loader_ishtp_cl);

	 
	cancel_work_sync(&client_data->work_fw_load);
	cancel_work_sync(&client_data->work_ishtp_reset);
	loader_deinit(loader_ishtp_cl);
	ishtp_put_device(cl_device);
}

 
static int loader_ishtp_cl_reset(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_data *client_data;
	struct ishtp_cl	*loader_ishtp_cl = ishtp_get_drvdata(cl_device);

	client_data = ishtp_get_client_data(loader_ishtp_cl);

	schedule_work(&client_data->work_ishtp_reset);

	return 0;
}

static struct ishtp_cl_driver	loader_ishtp_cl_driver = {
	.name = "ish-loader",
	.id = loader_ishtp_id_table,
	.probe = loader_ishtp_cl_probe,
	.remove = loader_ishtp_cl_remove,
	.reset = loader_ishtp_cl_reset,
};

static int __init ish_loader_init(void)
{
	return ishtp_cl_driver_register(&loader_ishtp_cl_driver, THIS_MODULE);
}

static void __exit ish_loader_exit(void)
{
	ishtp_cl_driver_unregister(&loader_ishtp_cl_driver);
}

late_initcall(ish_loader_init);
module_exit(ish_loader_exit);

module_param(dma_buf_size_limit, int, 0644);
MODULE_PARM_DESC(dma_buf_size_limit, "Limit the DMA buf size to this value in bytes");

MODULE_DESCRIPTION("ISH ISH-TP Host firmware Loader Client Driver");
MODULE_AUTHOR("Rushikesh S Kadam <rushikesh.s.kadam@intel.com>");

MODULE_LICENSE("GPL v2");
