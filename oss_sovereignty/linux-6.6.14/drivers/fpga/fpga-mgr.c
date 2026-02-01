
 
#include <linux/firmware.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>

static DEFINE_IDA(fpga_mgr_ida);
static const struct class fpga_mgr_class;

struct fpga_mgr_devres {
	struct fpga_manager *mgr;
};

static inline void fpga_mgr_fpga_remove(struct fpga_manager *mgr)
{
	if (mgr->mops->fpga_remove)
		mgr->mops->fpga_remove(mgr);
}

static inline enum fpga_mgr_states fpga_mgr_state(struct fpga_manager *mgr)
{
	if (mgr->mops->state)
		return  mgr->mops->state(mgr);
	return FPGA_MGR_STATE_UNKNOWN;
}

static inline u64 fpga_mgr_status(struct fpga_manager *mgr)
{
	if (mgr->mops->status)
		return mgr->mops->status(mgr);
	return 0;
}

static inline int fpga_mgr_write(struct fpga_manager *mgr, const char *buf, size_t count)
{
	if (mgr->mops->write)
		return  mgr->mops->write(mgr, buf, count);
	return -EOPNOTSUPP;
}

 
static inline int fpga_mgr_write_complete(struct fpga_manager *mgr,
					  struct fpga_image_info *info)
{
	int ret = 0;

	mgr->state = FPGA_MGR_STATE_WRITE_COMPLETE;
	if (mgr->mops->write_complete)
		ret = mgr->mops->write_complete(mgr, info);
	if (ret) {
		dev_err(&mgr->dev, "Error after writing image data to FPGA\n");
		mgr->state = FPGA_MGR_STATE_WRITE_COMPLETE_ERR;
		return ret;
	}
	mgr->state = FPGA_MGR_STATE_OPERATING;

	return 0;
}

static inline int fpga_mgr_parse_header(struct fpga_manager *mgr,
					struct fpga_image_info *info,
					const char *buf, size_t count)
{
	if (mgr->mops->parse_header)
		return mgr->mops->parse_header(mgr, info, buf, count);
	return 0;
}

static inline int fpga_mgr_write_init(struct fpga_manager *mgr,
				      struct fpga_image_info *info,
				      const char *buf, size_t count)
{
	if (mgr->mops->write_init)
		return  mgr->mops->write_init(mgr, info, buf, count);
	return 0;
}

static inline int fpga_mgr_write_sg(struct fpga_manager *mgr,
				    struct sg_table *sgt)
{
	if (mgr->mops->write_sg)
		return  mgr->mops->write_sg(mgr, sgt);
	return -EOPNOTSUPP;
}

 
struct fpga_image_info *fpga_image_info_alloc(struct device *dev)
{
	struct fpga_image_info *info;

	get_device(dev);

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		put_device(dev);
		return NULL;
	}

	info->dev = dev;

	return info;
}
EXPORT_SYMBOL_GPL(fpga_image_info_alloc);

 
void fpga_image_info_free(struct fpga_image_info *info)
{
	struct device *dev;

	if (!info)
		return;

	dev = info->dev;
	if (info->firmware_name)
		devm_kfree(dev, info->firmware_name);

	devm_kfree(dev, info);
	put_device(dev);
}
EXPORT_SYMBOL_GPL(fpga_image_info_free);

 
static int fpga_mgr_parse_header_mapped(struct fpga_manager *mgr,
					struct fpga_image_info *info,
					const char *buf, size_t count)
{
	int ret;

	mgr->state = FPGA_MGR_STATE_PARSE_HEADER;
	ret = fpga_mgr_parse_header(mgr, info, buf, count);

	if (info->header_size + info->data_size > count) {
		dev_err(&mgr->dev, "Bitstream data outruns FPGA image\n");
		ret = -EINVAL;
	}

	if (ret) {
		dev_err(&mgr->dev, "Error while parsing FPGA image header\n");
		mgr->state = FPGA_MGR_STATE_PARSE_HEADER_ERR;
	}

	return ret;
}

 
static int fpga_mgr_parse_header_sg_first(struct fpga_manager *mgr,
					  struct fpga_image_info *info,
					  struct sg_table *sgt)
{
	struct sg_mapping_iter miter;
	int ret;

	mgr->state = FPGA_MGR_STATE_PARSE_HEADER;

	sg_miter_start(&miter, sgt->sgl, sgt->nents, SG_MITER_FROM_SG);
	if (sg_miter_next(&miter) &&
	    miter.length >= info->header_size)
		ret = fpga_mgr_parse_header(mgr, info, miter.addr, miter.length);
	else
		ret = -EAGAIN;
	sg_miter_stop(&miter);

	if (ret && ret != -EAGAIN) {
		dev_err(&mgr->dev, "Error while parsing FPGA image header\n");
		mgr->state = FPGA_MGR_STATE_PARSE_HEADER_ERR;
	}

	return ret;
}

 
static void *fpga_mgr_parse_header_sg(struct fpga_manager *mgr,
				      struct fpga_image_info *info,
				      struct sg_table *sgt, size_t *ret_size)
{
	size_t len, new_header_size, header_size = 0;
	char *new_buf, *buf = NULL;
	int ret;

	do {
		new_header_size = info->header_size;
		if (new_header_size <= header_size) {
			dev_err(&mgr->dev, "Requested invalid header size\n");
			ret = -EFAULT;
			break;
		}

		new_buf = krealloc(buf, new_header_size, GFP_KERNEL);
		if (!new_buf) {
			ret = -ENOMEM;
			break;
		}

		buf = new_buf;

		len = sg_pcopy_to_buffer(sgt->sgl, sgt->nents,
					 buf + header_size,
					 new_header_size - header_size,
					 header_size);
		if (len != new_header_size - header_size) {
			ret = -EFAULT;
			break;
		}

		header_size = new_header_size;
		ret = fpga_mgr_parse_header(mgr, info, buf, header_size);
	} while (ret == -EAGAIN);

	if (ret) {
		dev_err(&mgr->dev, "Error while parsing FPGA image header\n");
		mgr->state = FPGA_MGR_STATE_PARSE_HEADER_ERR;
		kfree(buf);
		buf = ERR_PTR(ret);
	}

	*ret_size = header_size;

	return buf;
}

 
static int fpga_mgr_write_init_buf(struct fpga_manager *mgr,
				   struct fpga_image_info *info,
				   const char *buf, size_t count)
{
	size_t header_size = info->header_size;
	int ret;

	mgr->state = FPGA_MGR_STATE_WRITE_INIT;

	if (header_size > count)
		ret = -EINVAL;
	else if (!header_size)
		ret = fpga_mgr_write_init(mgr, info, NULL, 0);
	else
		ret = fpga_mgr_write_init(mgr, info, buf, count);

	if (ret) {
		dev_err(&mgr->dev, "Error preparing FPGA for writing\n");
		mgr->state = FPGA_MGR_STATE_WRITE_INIT_ERR;
		return ret;
	}

	return 0;
}

static int fpga_mgr_prepare_sg(struct fpga_manager *mgr,
			       struct fpga_image_info *info,
			       struct sg_table *sgt)
{
	struct sg_mapping_iter miter;
	size_t len;
	char *buf;
	int ret;

	 
	if (!mgr->mops->initial_header_size && !mgr->mops->parse_header)
		return fpga_mgr_write_init_buf(mgr, info, NULL, 0);

	 
	ret = fpga_mgr_parse_header_sg_first(mgr, info, sgt);
	 
	if (!ret) {
		sg_miter_start(&miter, sgt->sgl, sgt->nents, SG_MITER_FROM_SG);
		if (sg_miter_next(&miter)) {
			ret = fpga_mgr_write_init_buf(mgr, info, miter.addr,
						      miter.length);
			sg_miter_stop(&miter);
			return ret;
		}
		sg_miter_stop(&miter);
	 
	} else if (ret != -EAGAIN) {
		return ret;
	}

	 
	buf = fpga_mgr_parse_header_sg(mgr, info, sgt, &len);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	ret = fpga_mgr_write_init_buf(mgr, info, buf, len);

	kfree(buf);

	return ret;
}

 
static int fpga_mgr_buf_load_sg(struct fpga_manager *mgr,
				struct fpga_image_info *info,
				struct sg_table *sgt)
{
	int ret;

	ret = fpga_mgr_prepare_sg(mgr, info, sgt);
	if (ret)
		return ret;

	 
	mgr->state = FPGA_MGR_STATE_WRITE;
	if (mgr->mops->write_sg) {
		ret = fpga_mgr_write_sg(mgr, sgt);
	} else {
		size_t length, count = 0, data_size = info->data_size;
		struct sg_mapping_iter miter;

		sg_miter_start(&miter, sgt->sgl, sgt->nents, SG_MITER_FROM_SG);

		if (mgr->mops->skip_header &&
		    !sg_miter_skip(&miter, info->header_size)) {
			ret = -EINVAL;
			goto out;
		}

		while (sg_miter_next(&miter)) {
			if (data_size)
				length = min(miter.length, data_size - count);
			else
				length = miter.length;

			ret = fpga_mgr_write(mgr, miter.addr, length);
			if (ret)
				break;

			count += length;
			if (data_size && count >= data_size)
				break;
		}
		sg_miter_stop(&miter);
	}

out:
	if (ret) {
		dev_err(&mgr->dev, "Error while writing image data to FPGA\n");
		mgr->state = FPGA_MGR_STATE_WRITE_ERR;
		return ret;
	}

	return fpga_mgr_write_complete(mgr, info);
}

static int fpga_mgr_buf_load_mapped(struct fpga_manager *mgr,
				    struct fpga_image_info *info,
				    const char *buf, size_t count)
{
	int ret;

	ret = fpga_mgr_parse_header_mapped(mgr, info, buf, count);
	if (ret)
		return ret;

	ret = fpga_mgr_write_init_buf(mgr, info, buf, count);
	if (ret)
		return ret;

	if (mgr->mops->skip_header) {
		buf += info->header_size;
		count -= info->header_size;
	}

	if (info->data_size)
		count = info->data_size;

	 
	mgr->state = FPGA_MGR_STATE_WRITE;
	ret = fpga_mgr_write(mgr, buf, count);
	if (ret) {
		dev_err(&mgr->dev, "Error while writing image data to FPGA\n");
		mgr->state = FPGA_MGR_STATE_WRITE_ERR;
		return ret;
	}

	return fpga_mgr_write_complete(mgr, info);
}

 
static int fpga_mgr_buf_load(struct fpga_manager *mgr,
			     struct fpga_image_info *info,
			     const char *buf, size_t count)
{
	struct page **pages;
	struct sg_table sgt;
	const void *p;
	int nr_pages;
	int index;
	int rc;

	 
	if (mgr->mops->write)
		return fpga_mgr_buf_load_mapped(mgr, info, buf, count);

	 
	nr_pages = DIV_ROUND_UP((unsigned long)buf + count, PAGE_SIZE) -
		   (unsigned long)buf / PAGE_SIZE;
	pages = kmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	p = buf - offset_in_page(buf);
	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			kfree(pages);
			return -EFAULT;
		}
		p += PAGE_SIZE;
	}

	 
	rc = sg_alloc_table_from_pages(&sgt, pages, index, offset_in_page(buf),
				       count, GFP_KERNEL);
	kfree(pages);
	if (rc)
		return rc;

	rc = fpga_mgr_buf_load_sg(mgr, info, &sgt);
	sg_free_table(&sgt);

	return rc;
}

 
static int fpga_mgr_firmware_load(struct fpga_manager *mgr,
				  struct fpga_image_info *info,
				  const char *image_name)
{
	struct device *dev = &mgr->dev;
	const struct firmware *fw;
	int ret;

	dev_info(dev, "writing %s to %s\n", image_name, mgr->name);

	mgr->state = FPGA_MGR_STATE_FIRMWARE_REQ;

	ret = request_firmware(&fw, image_name, dev);
	if (ret) {
		mgr->state = FPGA_MGR_STATE_FIRMWARE_REQ_ERR;
		dev_err(dev, "Error requesting firmware %s\n", image_name);
		return ret;
	}

	ret = fpga_mgr_buf_load(mgr, info, fw->data, fw->size);

	release_firmware(fw);

	return ret;
}

 
int fpga_mgr_load(struct fpga_manager *mgr, struct fpga_image_info *info)
{
	info->header_size = mgr->mops->initial_header_size;

	if (info->sgt)
		return fpga_mgr_buf_load_sg(mgr, info, info->sgt);
	if (info->buf && info->count)
		return fpga_mgr_buf_load(mgr, info, info->buf, info->count);
	if (info->firmware_name)
		return fpga_mgr_firmware_load(mgr, info, info->firmware_name);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(fpga_mgr_load);

static const char * const state_str[] = {
	[FPGA_MGR_STATE_UNKNOWN] =		"unknown",
	[FPGA_MGR_STATE_POWER_OFF] =		"power off",
	[FPGA_MGR_STATE_POWER_UP] =		"power up",
	[FPGA_MGR_STATE_RESET] =		"reset",

	 
	[FPGA_MGR_STATE_FIRMWARE_REQ] =		"firmware request",
	[FPGA_MGR_STATE_FIRMWARE_REQ_ERR] =	"firmware request error",

	 
	[FPGA_MGR_STATE_PARSE_HEADER] =		"parse header",
	[FPGA_MGR_STATE_PARSE_HEADER_ERR] =	"parse header error",

	 
	[FPGA_MGR_STATE_WRITE_INIT] =		"write init",
	[FPGA_MGR_STATE_WRITE_INIT_ERR] =	"write init error",

	 
	[FPGA_MGR_STATE_WRITE] =		"write",
	[FPGA_MGR_STATE_WRITE_ERR] =		"write error",

	 
	[FPGA_MGR_STATE_WRITE_COMPLETE] =	"write complete",
	[FPGA_MGR_STATE_WRITE_COMPLETE_ERR] =	"write complete error",

	 
	[FPGA_MGR_STATE_OPERATING] =		"operating",
};

static ssize_t name_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct fpga_manager *mgr = to_fpga_manager(dev);

	return sprintf(buf, "%s\n", mgr->name);
}

static ssize_t state_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct fpga_manager *mgr = to_fpga_manager(dev);

	return sprintf(buf, "%s\n", state_str[mgr->state]);
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct fpga_manager *mgr = to_fpga_manager(dev);
	u64 status;
	int len = 0;

	status = fpga_mgr_status(mgr);

	if (status & FPGA_MGR_STATUS_OPERATION_ERR)
		len += sprintf(buf + len, "reconfig operation error\n");
	if (status & FPGA_MGR_STATUS_CRC_ERR)
		len += sprintf(buf + len, "reconfig CRC error\n");
	if (status & FPGA_MGR_STATUS_INCOMPATIBLE_IMAGE_ERR)
		len += sprintf(buf + len, "reconfig incompatible image\n");
	if (status & FPGA_MGR_STATUS_IP_PROTOCOL_ERR)
		len += sprintf(buf + len, "reconfig IP protocol error\n");
	if (status & FPGA_MGR_STATUS_FIFO_OVERFLOW_ERR)
		len += sprintf(buf + len, "reconfig fifo overflow error\n");

	return len;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(state);
static DEVICE_ATTR_RO(status);

static struct attribute *fpga_mgr_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_state.attr,
	&dev_attr_status.attr,
	NULL,
};
ATTRIBUTE_GROUPS(fpga_mgr);

static struct fpga_manager *__fpga_mgr_get(struct device *dev)
{
	struct fpga_manager *mgr;

	mgr = to_fpga_manager(dev);

	if (!try_module_get(dev->parent->driver->owner))
		goto err_dev;

	return mgr;

err_dev:
	put_device(dev);
	return ERR_PTR(-ENODEV);
}

static int fpga_mgr_dev_match(struct device *dev, const void *data)
{
	return dev->parent == data;
}

 
struct fpga_manager *fpga_mgr_get(struct device *dev)
{
	struct device *mgr_dev = class_find_device(&fpga_mgr_class, NULL, dev,
						   fpga_mgr_dev_match);
	if (!mgr_dev)
		return ERR_PTR(-ENODEV);

	return __fpga_mgr_get(mgr_dev);
}
EXPORT_SYMBOL_GPL(fpga_mgr_get);

 
struct fpga_manager *of_fpga_mgr_get(struct device_node *node)
{
	struct device *dev;

	dev = class_find_device_by_of_node(&fpga_mgr_class, node);
	if (!dev)
		return ERR_PTR(-ENODEV);

	return __fpga_mgr_get(dev);
}
EXPORT_SYMBOL_GPL(of_fpga_mgr_get);

 
void fpga_mgr_put(struct fpga_manager *mgr)
{
	module_put(mgr->dev.parent->driver->owner);
	put_device(&mgr->dev);
}
EXPORT_SYMBOL_GPL(fpga_mgr_put);

 
int fpga_mgr_lock(struct fpga_manager *mgr)
{
	if (!mutex_trylock(&mgr->ref_mutex)) {
		dev_err(&mgr->dev, "FPGA manager is in use.\n");
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fpga_mgr_lock);

 
void fpga_mgr_unlock(struct fpga_manager *mgr)
{
	mutex_unlock(&mgr->ref_mutex);
}
EXPORT_SYMBOL_GPL(fpga_mgr_unlock);

 
struct fpga_manager *
fpga_mgr_register_full(struct device *parent, const struct fpga_manager_info *info)
{
	const struct fpga_manager_ops *mops = info->mops;
	struct fpga_manager *mgr;
	int id, ret;

	if (!mops) {
		dev_err(parent, "Attempt to register without fpga_manager_ops\n");
		return ERR_PTR(-EINVAL);
	}

	if (!info->name || !strlen(info->name)) {
		dev_err(parent, "Attempt to register with no name!\n");
		return ERR_PTR(-EINVAL);
	}

	mgr = kzalloc(sizeof(*mgr), GFP_KERNEL);
	if (!mgr)
		return ERR_PTR(-ENOMEM);

	id = ida_alloc(&fpga_mgr_ida, GFP_KERNEL);
	if (id < 0) {
		ret = id;
		goto error_kfree;
	}

	mutex_init(&mgr->ref_mutex);

	mgr->name = info->name;
	mgr->mops = info->mops;
	mgr->priv = info->priv;
	mgr->compat_id = info->compat_id;

	mgr->dev.class = &fpga_mgr_class;
	mgr->dev.groups = mops->groups;
	mgr->dev.parent = parent;
	mgr->dev.of_node = parent->of_node;
	mgr->dev.id = id;

	ret = dev_set_name(&mgr->dev, "fpga%d", id);
	if (ret)
		goto error_device;

	 
	mgr->state = fpga_mgr_state(mgr);

	ret = device_register(&mgr->dev);
	if (ret) {
		put_device(&mgr->dev);
		return ERR_PTR(ret);
	}

	return mgr;

error_device:
	ida_free(&fpga_mgr_ida, id);
error_kfree:
	kfree(mgr);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(fpga_mgr_register_full);

 
struct fpga_manager *
fpga_mgr_register(struct device *parent, const char *name,
		  const struct fpga_manager_ops *mops, void *priv)
{
	struct fpga_manager_info info = { 0 };

	info.name = name;
	info.mops = mops;
	info.priv = priv;

	return fpga_mgr_register_full(parent, &info);
}
EXPORT_SYMBOL_GPL(fpga_mgr_register);

 
void fpga_mgr_unregister(struct fpga_manager *mgr)
{
	dev_info(&mgr->dev, "%s %s\n", __func__, mgr->name);

	 
	fpga_mgr_fpga_remove(mgr);

	device_unregister(&mgr->dev);
}
EXPORT_SYMBOL_GPL(fpga_mgr_unregister);

static void devm_fpga_mgr_unregister(struct device *dev, void *res)
{
	struct fpga_mgr_devres *dr = res;

	fpga_mgr_unregister(dr->mgr);
}

 
struct fpga_manager *
devm_fpga_mgr_register_full(struct device *parent, const struct fpga_manager_info *info)
{
	struct fpga_mgr_devres *dr;
	struct fpga_manager *mgr;

	dr = devres_alloc(devm_fpga_mgr_unregister, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return ERR_PTR(-ENOMEM);

	mgr = fpga_mgr_register_full(parent, info);
	if (IS_ERR(mgr)) {
		devres_free(dr);
		return mgr;
	}

	dr->mgr = mgr;
	devres_add(parent, dr);

	return mgr;
}
EXPORT_SYMBOL_GPL(devm_fpga_mgr_register_full);

 
struct fpga_manager *
devm_fpga_mgr_register(struct device *parent, const char *name,
		       const struct fpga_manager_ops *mops, void *priv)
{
	struct fpga_manager_info info = { 0 };

	info.name = name;
	info.mops = mops;
	info.priv = priv;

	return devm_fpga_mgr_register_full(parent, &info);
}
EXPORT_SYMBOL_GPL(devm_fpga_mgr_register);

static void fpga_mgr_dev_release(struct device *dev)
{
	struct fpga_manager *mgr = to_fpga_manager(dev);

	ida_free(&fpga_mgr_ida, mgr->dev.id);
	kfree(mgr);
}

static const struct class fpga_mgr_class = {
	.name = "fpga_manager",
	.dev_groups = fpga_mgr_groups,
	.dev_release = fpga_mgr_dev_release,
};

static int __init fpga_mgr_class_init(void)
{
	pr_info("FPGA manager framework\n");

	return class_register(&fpga_mgr_class);
}

static void __exit fpga_mgr_class_exit(void)
{
	class_unregister(&fpga_mgr_class);
	ida_destroy(&fpga_mgr_ida);
}

MODULE_AUTHOR("Alan Tull <atull@kernel.org>");
MODULE_DESCRIPTION("FPGA manager framework");
MODULE_LICENSE("GPL v2");

subsys_initcall(fpga_mgr_class_init);
module_exit(fpga_mgr_class_exit);
