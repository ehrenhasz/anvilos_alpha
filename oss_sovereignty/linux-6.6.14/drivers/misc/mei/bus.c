
 

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/mei_cl_bus.h>

#include "mei_dev.h"
#include "client.h"

#define to_mei_cl_driver(d) container_of(d, struct mei_cl_driver, driver)

 
ssize_t __mei_cl_send(struct mei_cl *cl, const u8 *buf, size_t length, u8 vtag,
		      unsigned int mode)
{
	return __mei_cl_send_timeout(cl, buf, length, vtag, mode, MAX_SCHEDULE_TIMEOUT);
}

 
ssize_t __mei_cl_send_timeout(struct mei_cl *cl, const u8 *buf, size_t length, u8 vtag,
			      unsigned int mode, unsigned long timeout)
{
	struct mei_device *bus;
	struct mei_cl_cb *cb;
	ssize_t rets;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	bus = cl->dev;

	mutex_lock(&bus->device_lock);
	if (bus->dev_state != MEI_DEV_ENABLED &&
	    bus->dev_state != MEI_DEV_POWERING_DOWN) {
		rets = -ENODEV;
		goto out;
	}

	if (!mei_cl_is_connected(cl)) {
		rets = -ENODEV;
		goto out;
	}

	 
	if (!mei_me_cl_is_active(cl->me_cl)) {
		rets = -ENOTTY;
		goto out;
	}

	if (vtag) {
		 
		rets = mei_cl_vt_support_check(cl);
		if (rets)
			goto out;
	}

	if (length > mei_cl_mtu(cl)) {
		rets = -EFBIG;
		goto out;
	}

	while (cl->tx_cb_queued >= bus->tx_queue_limit) {
		mutex_unlock(&bus->device_lock);
		rets = wait_event_interruptible(cl->tx_wait,
				cl->writing_state == MEI_WRITE_COMPLETE ||
				(!mei_cl_is_connected(cl)));
		mutex_lock(&bus->device_lock);
		if (rets) {
			if (signal_pending(current))
				rets = -EINTR;
			goto out;
		}
		if (!mei_cl_is_connected(cl)) {
			rets = -ENODEV;
			goto out;
		}
	}

	cb = mei_cl_alloc_cb(cl, length, MEI_FOP_WRITE, NULL);
	if (!cb) {
		rets = -ENOMEM;
		goto out;
	}
	cb->vtag = vtag;

	cb->internal = !!(mode & MEI_CL_IO_TX_INTERNAL);
	cb->blocking = !!(mode & MEI_CL_IO_TX_BLOCKING);
	memcpy(cb->buf.data, buf, length);
	 
	if (mode & MEI_CL_IO_SGL) {
		cb->ext_hdr = (struct mei_ext_hdr *)cb->buf.data;
		cb->buf.data = NULL;
		cb->buf.size = 0;
	}

	rets = mei_cl_write(cl, cb, timeout);

	if (mode & MEI_CL_IO_SGL && rets == 0)
		rets = length;

out:
	mutex_unlock(&bus->device_lock);

	return rets;
}

 
ssize_t __mei_cl_recv(struct mei_cl *cl, u8 *buf, size_t length, u8 *vtag,
		      unsigned int mode, unsigned long timeout)
{
	struct mei_device *bus;
	struct mei_cl_cb *cb;
	size_t r_length;
	ssize_t rets;
	bool nonblock = !!(mode & MEI_CL_IO_RX_NONBLOCK);

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	bus = cl->dev;

	mutex_lock(&bus->device_lock);
	if (bus->dev_state != MEI_DEV_ENABLED &&
	    bus->dev_state != MEI_DEV_POWERING_DOWN) {
		rets = -ENODEV;
		goto out;
	}

	cb = mei_cl_read_cb(cl, NULL);
	if (cb)
		goto copy;

	rets = mei_cl_read_start(cl, length, NULL);
	if (rets && rets != -EBUSY)
		goto out;

	if (nonblock) {
		rets = -EAGAIN;
		goto out;
	}

	 
	 
	if (!waitqueue_active(&cl->rx_wait)) {

		mutex_unlock(&bus->device_lock);

		if (timeout) {
			rets = wait_event_interruptible_timeout
					(cl->rx_wait,
					mei_cl_read_cb(cl, NULL) ||
					(!mei_cl_is_connected(cl)),
					msecs_to_jiffies(timeout));
			if (rets == 0)
				return -ETIME;
			if (rets < 0) {
				if (signal_pending(current))
					return -EINTR;
				return -ERESTARTSYS;
			}
		} else {
			if (wait_event_interruptible
					(cl->rx_wait,
					mei_cl_read_cb(cl, NULL) ||
					(!mei_cl_is_connected(cl)))) {
				if (signal_pending(current))
					return -EINTR;
				return -ERESTARTSYS;
			}
		}

		mutex_lock(&bus->device_lock);

		if (!mei_cl_is_connected(cl)) {
			rets = -ENODEV;
			goto out;
		}
	}

	cb = mei_cl_read_cb(cl, NULL);
	if (!cb) {
		rets = 0;
		goto out;
	}

copy:
	if (cb->status) {
		rets = cb->status;
		goto free;
	}

	 
	if (cb->ext_hdr && cb->ext_hdr->type == MEI_EXT_HDR_GSC) {
		r_length = min_t(size_t, length, cb->ext_hdr->length * sizeof(u32));
		memcpy(buf, cb->ext_hdr, r_length);
	} else {
		r_length = min_t(size_t, length, cb->buf_idx);
		memcpy(buf, cb->buf.data, r_length);
	}
	rets = r_length;

	if (vtag)
		*vtag = cb->vtag;

free:
	mei_cl_del_rd_completed(cl, cb);
out:
	mutex_unlock(&bus->device_lock);

	return rets;
}

 

ssize_t mei_cldev_send_vtag(struct mei_cl_device *cldev, const u8 *buf,
			    size_t length, u8 vtag)
{
	struct mei_cl *cl = cldev->cl;

	return __mei_cl_send(cl, buf, length, vtag, MEI_CL_IO_TX_BLOCKING);
}
EXPORT_SYMBOL_GPL(mei_cldev_send_vtag);

 

ssize_t mei_cldev_recv_vtag(struct mei_cl_device *cldev, u8 *buf, size_t length,
			    u8 *vtag)
{
	struct mei_cl *cl = cldev->cl;

	return __mei_cl_recv(cl, buf, length, vtag, 0, 0);
}
EXPORT_SYMBOL_GPL(mei_cldev_recv_vtag);

 
ssize_t mei_cldev_recv_nonblock_vtag(struct mei_cl_device *cldev, u8 *buf,
				     size_t length, u8 *vtag)
{
	struct mei_cl *cl = cldev->cl;

	return __mei_cl_recv(cl, buf, length, vtag, MEI_CL_IO_RX_NONBLOCK, 0);
}
EXPORT_SYMBOL_GPL(mei_cldev_recv_nonblock_vtag);

 
ssize_t mei_cldev_send(struct mei_cl_device *cldev, const u8 *buf, size_t length)
{
	return mei_cldev_send_vtag(cldev, buf, length, 0);
}
EXPORT_SYMBOL_GPL(mei_cldev_send);

 
ssize_t mei_cldev_recv(struct mei_cl_device *cldev, u8 *buf, size_t length)
{
	return mei_cldev_recv_vtag(cldev, buf, length, NULL);
}
EXPORT_SYMBOL_GPL(mei_cldev_recv);

 
ssize_t mei_cldev_recv_nonblock(struct mei_cl_device *cldev, u8 *buf,
				size_t length)
{
	return mei_cldev_recv_nonblock_vtag(cldev, buf, length, NULL);
}
EXPORT_SYMBOL_GPL(mei_cldev_recv_nonblock);

 
static void mei_cl_bus_rx_work(struct work_struct *work)
{
	struct mei_cl_device *cldev;
	struct mei_device *bus;

	cldev = container_of(work, struct mei_cl_device, rx_work);

	bus = cldev->bus;

	if (cldev->rx_cb)
		cldev->rx_cb(cldev);

	mutex_lock(&bus->device_lock);
	if (mei_cl_is_connected(cldev->cl))
		mei_cl_read_start(cldev->cl, mei_cl_mtu(cldev->cl), NULL);
	mutex_unlock(&bus->device_lock);
}

 
static void mei_cl_bus_notif_work(struct work_struct *work)
{
	struct mei_cl_device *cldev;

	cldev = container_of(work, struct mei_cl_device, notif_work);

	if (cldev->notif_cb)
		cldev->notif_cb(cldev);
}

 
bool mei_cl_bus_notify_event(struct mei_cl *cl)
{
	struct mei_cl_device *cldev = cl->cldev;

	if (!cldev || !cldev->notif_cb)
		return false;

	if (!cl->notify_ev)
		return false;

	schedule_work(&cldev->notif_work);

	cl->notify_ev = false;

	return true;
}

 
bool mei_cl_bus_rx_event(struct mei_cl *cl)
{
	struct mei_cl_device *cldev = cl->cldev;

	if (!cldev || !cldev->rx_cb)
		return false;

	schedule_work(&cldev->rx_work);

	return true;
}

 
int mei_cldev_register_rx_cb(struct mei_cl_device *cldev, mei_cldev_cb_t rx_cb)
{
	struct mei_device *bus = cldev->bus;
	int ret;

	if (!rx_cb)
		return -EINVAL;
	if (cldev->rx_cb)
		return -EALREADY;

	cldev->rx_cb = rx_cb;
	INIT_WORK(&cldev->rx_work, mei_cl_bus_rx_work);

	mutex_lock(&bus->device_lock);
	if (mei_cl_is_connected(cldev->cl))
		ret = mei_cl_read_start(cldev->cl, mei_cl_mtu(cldev->cl), NULL);
	else
		ret = -ENODEV;
	mutex_unlock(&bus->device_lock);
	if (ret && ret != -EBUSY) {
		cancel_work_sync(&cldev->rx_work);
		cldev->rx_cb = NULL;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mei_cldev_register_rx_cb);

 
int mei_cldev_register_notif_cb(struct mei_cl_device *cldev,
				mei_cldev_cb_t notif_cb)
{
	struct mei_device *bus = cldev->bus;
	int ret;

	if (!notif_cb)
		return -EINVAL;

	if (cldev->notif_cb)
		return -EALREADY;

	cldev->notif_cb = notif_cb;
	INIT_WORK(&cldev->notif_work, mei_cl_bus_notif_work);

	mutex_lock(&bus->device_lock);
	ret = mei_cl_notify_request(cldev->cl, NULL, 1);
	mutex_unlock(&bus->device_lock);
	if (ret) {
		cancel_work_sync(&cldev->notif_work);
		cldev->notif_cb = NULL;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mei_cldev_register_notif_cb);

 
void *mei_cldev_get_drvdata(const struct mei_cl_device *cldev)
{
	return dev_get_drvdata(&cldev->dev);
}
EXPORT_SYMBOL_GPL(mei_cldev_get_drvdata);

 
void mei_cldev_set_drvdata(struct mei_cl_device *cldev, void *data)
{
	dev_set_drvdata(&cldev->dev, data);
}
EXPORT_SYMBOL_GPL(mei_cldev_set_drvdata);

 
const uuid_le *mei_cldev_uuid(const struct mei_cl_device *cldev)
{
	return mei_me_cl_uuid(cldev->me_cl);
}
EXPORT_SYMBOL_GPL(mei_cldev_uuid);

 
u8 mei_cldev_ver(const struct mei_cl_device *cldev)
{
	return mei_me_cl_ver(cldev->me_cl);
}
EXPORT_SYMBOL_GPL(mei_cldev_ver);

 
bool mei_cldev_enabled(const struct mei_cl_device *cldev)
{
	return mei_cl_is_connected(cldev->cl);
}
EXPORT_SYMBOL_GPL(mei_cldev_enabled);

 
static bool mei_cl_bus_module_get(struct mei_cl_device *cldev)
{
	return try_module_get(cldev->bus->dev->driver->owner);
}

 
static void mei_cl_bus_module_put(struct mei_cl_device *cldev)
{
	module_put(cldev->bus->dev->driver->owner);
}

 
static inline struct mei_cl_vtag *mei_cl_bus_vtag(struct mei_cl *cl)
{
	return list_first_entry_or_null(&cl->vtag_map,
					struct mei_cl_vtag, list);
}

 
static int mei_cl_bus_vtag_alloc(struct mei_cl_device *cldev)
{
	struct mei_cl *cl = cldev->cl;
	struct mei_cl_vtag *cl_vtag;

	 
	if (mei_cl_vt_support_check(cl) || mei_cl_bus_vtag(cl))
		return 0;

	cl_vtag = mei_cl_vtag_alloc(NULL, 0);
	if (IS_ERR(cl_vtag))
		return -ENOMEM;

	list_add_tail(&cl_vtag->list, &cl->vtag_map);

	return 0;
}

 
static void mei_cl_bus_vtag_free(struct mei_cl_device *cldev)
{
	struct mei_cl *cl = cldev->cl;
	struct mei_cl_vtag *cl_vtag;

	cl_vtag = mei_cl_bus_vtag(cl);
	if (!cl_vtag)
		return;

	list_del(&cl_vtag->list);
	kfree(cl_vtag);
}

void *mei_cldev_dma_map(struct mei_cl_device *cldev, u8 buffer_id, size_t size)
{
	struct mei_device *bus;
	struct mei_cl *cl;
	int ret;

	if (!cldev || !buffer_id || !size)
		return ERR_PTR(-EINVAL);

	if (!IS_ALIGNED(size, MEI_FW_PAGE_SIZE)) {
		dev_err(&cldev->dev, "Map size should be aligned to %lu\n",
			MEI_FW_PAGE_SIZE);
		return ERR_PTR(-EINVAL);
	}

	cl = cldev->cl;
	bus = cldev->bus;

	mutex_lock(&bus->device_lock);
	if (cl->state == MEI_FILE_UNINITIALIZED) {
		ret = mei_cl_link(cl);
		if (ret)
			goto notlinked;
		 
		cl->cldev = cldev;
	}

	ret = mei_cl_dma_alloc_and_map(cl, NULL, buffer_id, size);
	if (ret)
		mei_cl_unlink(cl);
notlinked:
	mutex_unlock(&bus->device_lock);
	if (ret)
		return ERR_PTR(ret);
	return cl->dma.vaddr;
}
EXPORT_SYMBOL_GPL(mei_cldev_dma_map);

int mei_cldev_dma_unmap(struct mei_cl_device *cldev)
{
	struct mei_device *bus;
	struct mei_cl *cl;
	int ret;

	if (!cldev)
		return -EINVAL;

	cl = cldev->cl;
	bus = cldev->bus;

	mutex_lock(&bus->device_lock);
	ret = mei_cl_dma_unmap(cl, NULL);

	mei_cl_flush_queues(cl, NULL);
	mei_cl_unlink(cl);
	mutex_unlock(&bus->device_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(mei_cldev_dma_unmap);

 
int mei_cldev_enable(struct mei_cl_device *cldev)
{
	struct mei_device *bus = cldev->bus;
	struct mei_cl *cl;
	int ret;

	cl = cldev->cl;

	mutex_lock(&bus->device_lock);
	if (cl->state == MEI_FILE_UNINITIALIZED) {
		ret = mei_cl_link(cl);
		if (ret)
			goto notlinked;
		 
		cl->cldev = cldev;
	}

	if (mei_cl_is_connected(cl)) {
		ret = 0;
		goto out;
	}

	if (!mei_me_cl_is_active(cldev->me_cl)) {
		dev_err(&cldev->dev, "me client is not active\n");
		ret = -ENOTTY;
		goto out;
	}

	ret = mei_cl_bus_vtag_alloc(cldev);
	if (ret)
		goto out;

	ret = mei_cl_connect(cl, cldev->me_cl, NULL);
	if (ret < 0) {
		dev_err(&cldev->dev, "cannot connect\n");
		mei_cl_bus_vtag_free(cldev);
	}

out:
	if (ret)
		mei_cl_unlink(cl);
notlinked:
	mutex_unlock(&bus->device_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mei_cldev_enable);

 
static void mei_cldev_unregister_callbacks(struct mei_cl_device *cldev)
{
	if (cldev->rx_cb) {
		cancel_work_sync(&cldev->rx_work);
		cldev->rx_cb = NULL;
	}

	if (cldev->notif_cb) {
		cancel_work_sync(&cldev->notif_work);
		cldev->notif_cb = NULL;
	}
}

 
int mei_cldev_disable(struct mei_cl_device *cldev)
{
	struct mei_device *bus;
	struct mei_cl *cl;
	int err;

	if (!cldev)
		return -ENODEV;

	cl = cldev->cl;

	bus = cldev->bus;

	mei_cldev_unregister_callbacks(cldev);

	mutex_lock(&bus->device_lock);

	mei_cl_bus_vtag_free(cldev);

	if (!mei_cl_is_connected(cl)) {
		dev_dbg(bus->dev, "Already disconnected\n");
		err = 0;
		goto out;
	}

	err = mei_cl_disconnect(cl);
	if (err < 0)
		dev_err(bus->dev, "Could not disconnect from the ME client\n");

out:
	 
	if (!cl->dma_mapped) {
		mei_cl_flush_queues(cl, NULL);
		mei_cl_unlink(cl);
	}

	mutex_unlock(&bus->device_lock);
	return err;
}
EXPORT_SYMBOL_GPL(mei_cldev_disable);

 
ssize_t mei_cldev_send_gsc_command(struct mei_cl_device *cldev,
				   u8 client_id, u32 fence_id,
				   struct scatterlist *sg_in,
				   size_t total_in_len,
				   struct scatterlist *sg_out)
{
	struct mei_cl *cl;
	struct mei_device *bus;
	ssize_t ret = 0;

	struct mei_ext_hdr_gsc_h2f *ext_hdr;
	size_t buf_sz = sizeof(struct mei_ext_hdr_gsc_h2f);
	int sg_out_nents, sg_in_nents;
	int i;
	struct scatterlist *sg;
	struct mei_ext_hdr_gsc_f2h rx_msg;
	unsigned int sg_len;

	if (!cldev || !sg_in || !sg_out)
		return -EINVAL;

	cl = cldev->cl;
	bus = cldev->bus;

	dev_dbg(bus->dev, "client_id %u, fence_id %u\n", client_id, fence_id);

	if (!bus->hbm_f_gsc_supported)
		return -EOPNOTSUPP;

	sg_out_nents = sg_nents(sg_out);
	sg_in_nents = sg_nents(sg_in);
	 
	if (sg_out_nents <= 0 || sg_in_nents <= 0)
		return -EINVAL;

	buf_sz += (sg_out_nents + sg_in_nents) * sizeof(struct mei_gsc_sgl);
	ext_hdr = kzalloc(buf_sz, GFP_KERNEL);
	if (!ext_hdr)
		return -ENOMEM;

	 
	ext_hdr->hdr.type = MEI_EXT_HDR_GSC;
	ext_hdr->hdr.length = buf_sz / sizeof(u32);  

	ext_hdr->client_id = client_id;
	ext_hdr->addr_type = GSC_ADDRESS_TYPE_PHYSICAL_SGL;
	ext_hdr->fence_id = fence_id;
	ext_hdr->input_address_count = sg_in_nents;
	ext_hdr->output_address_count = sg_out_nents;
	ext_hdr->reserved[0] = 0;
	ext_hdr->reserved[1] = 0;

	 
	for (i = 0, sg = sg_in; i < sg_in_nents; i++, sg++) {
		ext_hdr->sgl[i].low = lower_32_bits(sg_dma_address(sg));
		ext_hdr->sgl[i].high = upper_32_bits(sg_dma_address(sg));
		sg_len = min_t(unsigned int, sg_dma_len(sg), PAGE_SIZE);
		ext_hdr->sgl[i].length = (sg_len <= total_in_len) ? sg_len : total_in_len;
		total_in_len -= ext_hdr->sgl[i].length;
	}

	 
	for (i = sg_in_nents, sg = sg_out; i < sg_in_nents + sg_out_nents; i++, sg++) {
		ext_hdr->sgl[i].low = lower_32_bits(sg_dma_address(sg));
		ext_hdr->sgl[i].high = upper_32_bits(sg_dma_address(sg));
		sg_len = min_t(unsigned int, sg_dma_len(sg), PAGE_SIZE);
		ext_hdr->sgl[i].length = sg_len;
	}

	 
	ret = __mei_cl_send(cl, (u8 *)ext_hdr, buf_sz, 0, MEI_CL_IO_SGL);
	if (ret < 0) {
		dev_err(bus->dev, "__mei_cl_send failed, returned %zd\n", ret);
		goto end;
	}
	if (ret != buf_sz) {
		dev_err(bus->dev, "__mei_cl_send returned %zd instead of expected %zd\n",
			ret, buf_sz);
		ret = -EIO;
		goto end;
	}

	 
	ret = __mei_cl_recv(cl, (u8 *)&rx_msg, sizeof(rx_msg), NULL, MEI_CL_IO_SGL, 0);

	if (ret != sizeof(rx_msg)) {
		dev_err(bus->dev, "__mei_cl_recv returned %zd instead of expected %zd\n",
			ret, sizeof(rx_msg));
		if (ret >= 0)
			ret = -EIO;
		goto end;
	}

	 
	if (rx_msg.client_id != client_id || rx_msg.fence_id != fence_id) {
		dev_err(bus->dev, "received client_id/fence_id  %u/%u  instead of %u/%u sent\n",
			rx_msg.client_id, rx_msg.fence_id, client_id, fence_id);
		ret = -EFAULT;
		goto end;
	}

	dev_dbg(bus->dev, "gsc command: successfully written %u bytes\n",  rx_msg.written);
	ret = rx_msg.written;

end:
	kfree(ext_hdr);
	return ret;
}
EXPORT_SYMBOL_GPL(mei_cldev_send_gsc_command);

 
static const
struct mei_cl_device_id *mei_cl_device_find(const struct mei_cl_device *cldev,
					    const struct mei_cl_driver *cldrv)
{
	const struct mei_cl_device_id *id;
	const uuid_le *uuid;
	u8 version;
	bool match;

	uuid = mei_me_cl_uuid(cldev->me_cl);
	version = mei_me_cl_ver(cldev->me_cl);

	id = cldrv->id_table;
	while (uuid_le_cmp(NULL_UUID_LE, id->uuid)) {
		if (!uuid_le_cmp(*uuid, id->uuid)) {
			match = true;

			if (cldev->name[0])
				if (strncmp(cldev->name, id->name,
					    sizeof(id->name)))
					match = false;

			if (id->version != MEI_CL_VERSION_ANY)
				if (id->version != version)
					match = false;
			if (match)
				return id;
		}

		id++;
	}

	return NULL;
}

 
static int mei_cl_device_match(struct device *dev, struct device_driver *drv)
{
	const struct mei_cl_device *cldev = to_mei_cl_device(dev);
	const struct mei_cl_driver *cldrv = to_mei_cl_driver(drv);
	const struct mei_cl_device_id *found_id;

	if (!cldev->do_match)
		return 0;

	if (!cldrv || !cldrv->id_table)
		return 0;

	found_id = mei_cl_device_find(cldev, cldrv);
	if (found_id)
		return 1;

	return 0;
}

 
static int mei_cl_device_probe(struct device *dev)
{
	struct mei_cl_device *cldev;
	struct mei_cl_driver *cldrv;
	const struct mei_cl_device_id *id;
	int ret;

	cldev = to_mei_cl_device(dev);
	cldrv = to_mei_cl_driver(dev->driver);

	if (!cldrv || !cldrv->probe)
		return -ENODEV;

	id = mei_cl_device_find(cldev, cldrv);
	if (!id)
		return -ENODEV;

	if (!mei_cl_bus_module_get(cldev)) {
		dev_err(&cldev->dev, "get hw module failed");
		return -ENODEV;
	}

	ret = cldrv->probe(cldev, id);
	if (ret) {
		mei_cl_bus_module_put(cldev);
		return ret;
	}

	__module_get(THIS_MODULE);
	return 0;
}

 
static void mei_cl_device_remove(struct device *dev)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	struct mei_cl_driver *cldrv = to_mei_cl_driver(dev->driver);

	if (cldrv->remove)
		cldrv->remove(cldev);

	mei_cldev_unregister_callbacks(cldev);

	mei_cl_bus_module_put(cldev);
	module_put(THIS_MODULE);
}

static ssize_t name_show(struct device *dev, struct device_attribute *a,
			     char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%s", cldev->name);
}
static DEVICE_ATTR_RO(name);

static ssize_t uuid_show(struct device *dev, struct device_attribute *a,
			     char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	const uuid_le *uuid = mei_me_cl_uuid(cldev->me_cl);

	return sprintf(buf, "%pUl", uuid);
}
static DEVICE_ATTR_RO(uuid);

static ssize_t version_show(struct device *dev, struct device_attribute *a,
			     char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	u8 version = mei_me_cl_ver(cldev->me_cl);

	return sprintf(buf, "%02X", version);
}
static DEVICE_ATTR_RO(version);

static ssize_t modalias_show(struct device *dev, struct device_attribute *a,
			     char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	const uuid_le *uuid = mei_me_cl_uuid(cldev->me_cl);
	u8 version = mei_me_cl_ver(cldev->me_cl);

	return scnprintf(buf, PAGE_SIZE, "mei:%s:%pUl:%02X:",
			 cldev->name, uuid, version);
}
static DEVICE_ATTR_RO(modalias);

static ssize_t max_conn_show(struct device *dev, struct device_attribute *a,
			     char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	u8 maxconn = mei_me_cl_max_conn(cldev->me_cl);

	return sprintf(buf, "%d", maxconn);
}
static DEVICE_ATTR_RO(max_conn);

static ssize_t fixed_show(struct device *dev, struct device_attribute *a,
			  char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	u8 fixed = mei_me_cl_fixed(cldev->me_cl);

	return sprintf(buf, "%d", fixed);
}
static DEVICE_ATTR_RO(fixed);

static ssize_t vtag_show(struct device *dev, struct device_attribute *a,
			 char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	bool vt = mei_me_cl_vt(cldev->me_cl);

	return sprintf(buf, "%d", vt);
}
static DEVICE_ATTR_RO(vtag);

static ssize_t max_len_show(struct device *dev, struct device_attribute *a,
			    char *buf)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);
	u32 maxlen = mei_me_cl_max_len(cldev->me_cl);

	return sprintf(buf, "%u", maxlen);
}
static DEVICE_ATTR_RO(max_len);

static struct attribute *mei_cldev_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_uuid.attr,
	&dev_attr_version.attr,
	&dev_attr_modalias.attr,
	&dev_attr_max_conn.attr,
	&dev_attr_fixed.attr,
	&dev_attr_vtag.attr,
	&dev_attr_max_len.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mei_cldev);

 
static int mei_cl_device_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct mei_cl_device *cldev = to_mei_cl_device(dev);
	const uuid_le *uuid = mei_me_cl_uuid(cldev->me_cl);
	u8 version = mei_me_cl_ver(cldev->me_cl);

	if (add_uevent_var(env, "MEI_CL_VERSION=%d", version))
		return -ENOMEM;

	if (add_uevent_var(env, "MEI_CL_UUID=%pUl", uuid))
		return -ENOMEM;

	if (add_uevent_var(env, "MEI_CL_NAME=%s", cldev->name))
		return -ENOMEM;

	if (add_uevent_var(env, "MODALIAS=mei:%s:%pUl:%02X:",
			   cldev->name, uuid, version))
		return -ENOMEM;

	return 0;
}

static struct bus_type mei_cl_bus_type = {
	.name		= "mei",
	.dev_groups	= mei_cldev_groups,
	.match		= mei_cl_device_match,
	.probe		= mei_cl_device_probe,
	.remove		= mei_cl_device_remove,
	.uevent		= mei_cl_device_uevent,
};

static struct mei_device *mei_dev_bus_get(struct mei_device *bus)
{
	if (bus)
		get_device(bus->dev);

	return bus;
}

static void mei_dev_bus_put(struct mei_device *bus)
{
	if (bus)
		put_device(bus->dev);
}

static void mei_cl_bus_dev_release(struct device *dev)
{
	struct mei_cl_device *cldev = to_mei_cl_device(dev);

	mei_cl_flush_queues(cldev->cl, NULL);
	mei_me_cl_put(cldev->me_cl);
	mei_dev_bus_put(cldev->bus);
	kfree(cldev->cl);
	kfree(cldev);
}

static const struct device_type mei_cl_device_type = {
	.release = mei_cl_bus_dev_release,
};

 
static inline void mei_cl_bus_set_name(struct mei_cl_device *cldev)
{
	dev_set_name(&cldev->dev, "%s-%pUl",
		     dev_name(cldev->bus->dev),
		     mei_me_cl_uuid(cldev->me_cl));
}

 
static struct mei_cl_device *mei_cl_bus_dev_alloc(struct mei_device *bus,
						  struct mei_me_client *me_cl)
{
	struct mei_cl_device *cldev;
	struct mei_cl *cl;

	cldev = kzalloc(sizeof(*cldev), GFP_KERNEL);
	if (!cldev)
		return NULL;

	cl = mei_cl_allocate(bus);
	if (!cl) {
		kfree(cldev);
		return NULL;
	}

	device_initialize(&cldev->dev);
	cldev->dev.parent = bus->dev;
	cldev->dev.bus    = &mei_cl_bus_type;
	cldev->dev.type   = &mei_cl_device_type;
	cldev->bus        = mei_dev_bus_get(bus);
	cldev->me_cl      = mei_me_cl_get(me_cl);
	cldev->cl         = cl;
	mei_cl_bus_set_name(cldev);
	cldev->is_added   = 0;
	INIT_LIST_HEAD(&cldev->bus_list);
	device_enable_async_suspend(&cldev->dev);

	return cldev;
}

 
static bool mei_cl_bus_dev_setup(struct mei_device *bus,
				 struct mei_cl_device *cldev)
{
	cldev->do_match = 1;
	mei_cl_bus_dev_fixup(cldev);

	 
	if (cldev->do_match)
		mei_cl_bus_set_name(cldev);

	return cldev->do_match == 1;
}

 
static int mei_cl_bus_dev_add(struct mei_cl_device *cldev)
{
	int ret;

	dev_dbg(cldev->bus->dev, "adding %pUL:%02X\n",
		mei_me_cl_uuid(cldev->me_cl),
		mei_me_cl_ver(cldev->me_cl));
	ret = device_add(&cldev->dev);
	if (!ret)
		cldev->is_added = 1;

	return ret;
}

 
static void mei_cl_bus_dev_stop(struct mei_cl_device *cldev)
{
	cldev->do_match = 0;
	if (cldev->is_added)
		device_release_driver(&cldev->dev);
}

 
static void mei_cl_bus_dev_destroy(struct mei_cl_device *cldev)
{

	WARN_ON(!mutex_is_locked(&cldev->bus->cl_bus_lock));

	if (!cldev->is_added)
		return;

	device_del(&cldev->dev);

	list_del_init(&cldev->bus_list);

	cldev->is_added = 0;
	put_device(&cldev->dev);
}

 
static void mei_cl_bus_remove_device(struct mei_cl_device *cldev)
{
	mei_cl_bus_dev_stop(cldev);
	mei_cl_bus_dev_destroy(cldev);
}

 
void mei_cl_bus_remove_devices(struct mei_device *bus)
{
	struct mei_cl_device *cldev, *next;

	mutex_lock(&bus->cl_bus_lock);
	list_for_each_entry_safe(cldev, next, &bus->device_list, bus_list)
		mei_cl_bus_remove_device(cldev);
	mutex_unlock(&bus->cl_bus_lock);
}


 
static void mei_cl_bus_dev_init(struct mei_device *bus,
				struct mei_me_client *me_cl)
{
	struct mei_cl_device *cldev;

	WARN_ON(!mutex_is_locked(&bus->cl_bus_lock));

	dev_dbg(bus->dev, "initializing %pUl", mei_me_cl_uuid(me_cl));

	if (me_cl->bus_added)
		return;

	cldev = mei_cl_bus_dev_alloc(bus, me_cl);
	if (!cldev)
		return;

	me_cl->bus_added = true;
	list_add_tail(&cldev->bus_list, &bus->device_list);

}

 
static void mei_cl_bus_rescan(struct mei_device *bus)
{
	struct mei_cl_device *cldev, *n;
	struct mei_me_client *me_cl;

	mutex_lock(&bus->cl_bus_lock);

	down_read(&bus->me_clients_rwsem);
	list_for_each_entry(me_cl, &bus->me_clients, list)
		mei_cl_bus_dev_init(bus, me_cl);
	up_read(&bus->me_clients_rwsem);

	list_for_each_entry_safe(cldev, n, &bus->device_list, bus_list) {

		if (!mei_me_cl_is_active(cldev->me_cl)) {
			mei_cl_bus_remove_device(cldev);
			continue;
		}

		if (cldev->is_added)
			continue;

		if (mei_cl_bus_dev_setup(bus, cldev))
			mei_cl_bus_dev_add(cldev);
		else {
			list_del_init(&cldev->bus_list);
			put_device(&cldev->dev);
		}
	}
	mutex_unlock(&bus->cl_bus_lock);

	dev_dbg(bus->dev, "rescan end");
}

void mei_cl_bus_rescan_work(struct work_struct *work)
{
	struct mei_device *bus =
		container_of(work, struct mei_device, bus_rescan_work);

	mei_cl_bus_rescan(bus);
}

int __mei_cldev_driver_register(struct mei_cl_driver *cldrv,
				struct module *owner)
{
	int err;

	cldrv->driver.name = cldrv->name;
	cldrv->driver.owner = owner;
	cldrv->driver.bus = &mei_cl_bus_type;

	err = driver_register(&cldrv->driver);
	if (err)
		return err;

	pr_debug("mei: driver [%s] registered\n", cldrv->driver.name);

	return 0;
}
EXPORT_SYMBOL_GPL(__mei_cldev_driver_register);

void mei_cldev_driver_unregister(struct mei_cl_driver *cldrv)
{
	driver_unregister(&cldrv->driver);

	pr_debug("mei: driver [%s] unregistered\n", cldrv->driver.name);
}
EXPORT_SYMBOL_GPL(mei_cldev_driver_unregister);


int __init mei_cl_bus_init(void)
{
	return bus_register(&mei_cl_bus_type);
}

void __exit mei_cl_bus_exit(void)
{
	bus_unregister(&mei_cl_bus_type);
}
