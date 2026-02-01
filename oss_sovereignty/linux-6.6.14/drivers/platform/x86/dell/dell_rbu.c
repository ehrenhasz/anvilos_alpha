
 

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/blkdev.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <asm/set_memory.h>

MODULE_AUTHOR("Abhay Salunke <abhay_salunke@dell.com>");
MODULE_DESCRIPTION("Driver for updating BIOS image on DELL systems");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.2");

#define BIOS_SCAN_LIMIT 0xffffffff
#define MAX_IMAGE_LENGTH 16
static struct _rbu_data {
	void *image_update_buffer;
	unsigned long image_update_buffer_size;
	unsigned long bios_image_size;
	int image_update_ordernum;
	spinlock_t lock;
	unsigned long packet_read_count;
	unsigned long num_packets;
	unsigned long packetsize;
	unsigned long imagesize;
	int entry_created;
} rbu_data;

static char image_type[MAX_IMAGE_LENGTH + 1] = "mono";
module_param_string(image_type, image_type, sizeof (image_type), 0);
MODULE_PARM_DESC(image_type, "BIOS image type. choose- mono or packet or init");

static unsigned long allocation_floor = 0x100000;
module_param(allocation_floor, ulong, 0644);
MODULE_PARM_DESC(allocation_floor, "Minimum address for allocations when using Packet mode");

struct packet_data {
	struct list_head list;
	size_t length;
	void *data;
	int ordernum;
};

static struct packet_data packet_data_head;

static struct platform_device *rbu_device;
static int context;

static void init_packet_head(void)
{
	INIT_LIST_HEAD(&packet_data_head.list);
	rbu_data.packet_read_count = 0;
	rbu_data.num_packets = 0;
	rbu_data.packetsize = 0;
	rbu_data.imagesize = 0;
}

static int create_packet(void *data, size_t length)
{
	struct packet_data *newpacket;
	int ordernum = 0;
	int retval = 0;
	unsigned int packet_array_size = 0;
	void **invalid_addr_packet_array = NULL;
	void *packet_data_temp_buf = NULL;
	unsigned int idx = 0;

	pr_debug("entry\n");

	if (!rbu_data.packetsize) {
		pr_debug("packetsize not specified\n");
		retval = -EINVAL;
		goto out_noalloc;
	}

	spin_unlock(&rbu_data.lock);

	newpacket = kzalloc(sizeof (struct packet_data), GFP_KERNEL);

	if (!newpacket) {
		pr_warn("failed to allocate new packet\n");
		retval = -ENOMEM;
		spin_lock(&rbu_data.lock);
		goto out_noalloc;
	}

	ordernum = get_order(length);

	 
	packet_array_size = max_t(unsigned int, allocation_floor / rbu_data.packetsize, 1);
	invalid_addr_packet_array = kcalloc(packet_array_size, sizeof(void *),
						GFP_KERNEL);

	if (!invalid_addr_packet_array) {
		pr_warn("failed to allocate invalid_addr_packet_array\n");
		retval = -ENOMEM;
		spin_lock(&rbu_data.lock);
		goto out_alloc_packet;
	}

	while (!packet_data_temp_buf) {
		packet_data_temp_buf = (unsigned char *)
			__get_free_pages(GFP_KERNEL, ordernum);
		if (!packet_data_temp_buf) {
			pr_warn("failed to allocate new packet\n");
			retval = -ENOMEM;
			spin_lock(&rbu_data.lock);
			goto out_alloc_packet_array;
		}

		if ((unsigned long)virt_to_phys(packet_data_temp_buf)
				< allocation_floor) {
			pr_debug("packet 0x%lx below floor at 0x%lx\n",
					(unsigned long)virt_to_phys(
						packet_data_temp_buf),
					allocation_floor);
			invalid_addr_packet_array[idx++] = packet_data_temp_buf;
			packet_data_temp_buf = NULL;
		}
	}
	 
	set_memory_uc((unsigned long)packet_data_temp_buf, 1 << ordernum);

	spin_lock(&rbu_data.lock);

	newpacket->data = packet_data_temp_buf;

	pr_debug("newpacket at physical addr %lx\n",
		(unsigned long)virt_to_phys(newpacket->data));

	 
	newpacket->length = length;
	newpacket->ordernum = ordernum;
	++rbu_data.num_packets;

	 
	INIT_LIST_HEAD(&newpacket->list);
	list_add_tail(&newpacket->list, &packet_data_head.list);

	memcpy(newpacket->data, data, length);

	pr_debug("exit\n");

out_alloc_packet_array:
	 
	while (idx--) {
		pr_debug("freeing unused packet below floor 0x%lx\n",
			(unsigned long)virt_to_phys(invalid_addr_packet_array[idx]));
		free_pages((unsigned long)invalid_addr_packet_array[idx], ordernum);
	}
	kfree(invalid_addr_packet_array);

out_alloc_packet:
	 
	if (retval)
		kfree(newpacket);

out_noalloc:
	return retval;
}

static int packetize_data(const u8 *data, size_t length)
{
	int rc = 0;
	int done = 0;
	int packet_length;
	u8 *temp;
	u8 *end = (u8 *) data + length;
	pr_debug("data length %zd\n", length);
	if (!rbu_data.packetsize) {
		pr_warn("packetsize not specified\n");
		return -EIO;
	}

	temp = (u8 *) data;

	 
	while (!done) {
		if ((temp + rbu_data.packetsize) < end)
			packet_length = rbu_data.packetsize;
		else {
			 
			packet_length = end - temp;
			done = 1;
		}

		if ((rc = create_packet(temp, packet_length)))
			return rc;

		pr_debug("%p:%td\n", temp, (end - temp));
		temp += packet_length;
	}

	rbu_data.imagesize = length;

	return rc;
}

static int do_packet_read(char *data, struct packet_data *newpacket,
	int length, int bytes_read, int *list_read_count)
{
	void *ptemp_buf;
	int bytes_copied = 0;
	int j = 0;

	*list_read_count += newpacket->length;

	if (*list_read_count > bytes_read) {
		 
		j = newpacket->length - (*list_read_count - bytes_read);
		 
		ptemp_buf = (u8 *) newpacket->data + j;
		 
		if (length > (*list_read_count - bytes_read))
			 
			bytes_copied = (*list_read_count - bytes_read);
		else
			 
			bytes_copied = length;
		memcpy(data, ptemp_buf, bytes_copied);
	}
	return bytes_copied;
}

static int packet_read_list(char *data, size_t * pread_length)
{
	struct packet_data *newpacket;
	int temp_count = 0;
	int bytes_copied = 0;
	int bytes_read = 0;
	int remaining_bytes = 0;
	char *pdest = data;

	 
	if (0 == rbu_data.num_packets)
		return -ENOMEM;

	remaining_bytes = *pread_length;
	bytes_read = rbu_data.packet_read_count;

	list_for_each_entry(newpacket, (&packet_data_head.list)->next, list) {
		bytes_copied = do_packet_read(pdest, newpacket,
			remaining_bytes, bytes_read, &temp_count);
		remaining_bytes -= bytes_copied;
		bytes_read += bytes_copied;
		pdest += bytes_copied;
		 
		if (remaining_bytes == 0)
			break;
	}
	 
	*pread_length = bytes_read - rbu_data.packet_read_count;
	rbu_data.packet_read_count = bytes_read;
	return 0;
}

static void packet_empty_list(void)
{
	struct packet_data *newpacket, *tmp;

	list_for_each_entry_safe(newpacket, tmp, (&packet_data_head.list)->next, list) {
		list_del(&newpacket->list);

		 
		memset(newpacket->data, 0, rbu_data.packetsize);
		set_memory_wb((unsigned long)newpacket->data,
			1 << newpacket->ordernum);
		free_pages((unsigned long) newpacket->data,
			newpacket->ordernum);
		kfree(newpacket);
	}
	rbu_data.packet_read_count = 0;
	rbu_data.num_packets = 0;
	rbu_data.imagesize = 0;
}

 
static void img_update_free(void)
{
	if (!rbu_data.image_update_buffer)
		return;
	 
	memset(rbu_data.image_update_buffer, 0,
		rbu_data.image_update_buffer_size);
	free_pages((unsigned long) rbu_data.image_update_buffer,
		rbu_data.image_update_ordernum);

	 
	rbu_data.image_update_ordernum = -1;
	rbu_data.image_update_buffer = NULL;
	rbu_data.image_update_buffer_size = 0;
	rbu_data.bios_image_size = 0;
}

 
static int img_update_realloc(unsigned long size)
{
	unsigned char *image_update_buffer = NULL;
	unsigned long img_buf_phys_addr;
	int ordernum;

	 
	if (rbu_data.image_update_buffer_size >= size) {
		 
		if ((size != 0) && (rbu_data.image_update_buffer == NULL)) {
			pr_err("corruption check failed\n");
			return -EINVAL;
		}
		 
		return 0;
	}

	 
	img_update_free();

	spin_unlock(&rbu_data.lock);

	ordernum = get_order(size);
	image_update_buffer =
		(unsigned char *)__get_free_pages(GFP_DMA32, ordernum);
	spin_lock(&rbu_data.lock);
	if (!image_update_buffer) {
		pr_debug("Not enough memory for image update: size = %ld\n", size);
		return -ENOMEM;
	}

	img_buf_phys_addr = (unsigned long)virt_to_phys(image_update_buffer);
	if (WARN_ON_ONCE(img_buf_phys_addr > BIOS_SCAN_LIMIT))
		return -EINVAL;  

	rbu_data.image_update_buffer = image_update_buffer;
	rbu_data.image_update_buffer_size = size;
	rbu_data.bios_image_size = rbu_data.image_update_buffer_size;
	rbu_data.image_update_ordernum = ordernum;
	return 0;
}

static ssize_t read_packet_data(char *buffer, loff_t pos, size_t count)
{
	int retval;
	size_t bytes_left;
	size_t data_length;
	char *ptempBuf = buffer;

	 
	if (rbu_data.num_packets == 0) {
		pr_debug("no packets written\n");
		retval = -ENOMEM;
		goto read_rbu_data_exit;
	}

	if (pos > rbu_data.imagesize) {
		retval = 0;
		pr_warn("data underrun\n");
		goto read_rbu_data_exit;
	}

	bytes_left = rbu_data.imagesize - pos;
	data_length = min(bytes_left, count);

	if ((retval = packet_read_list(ptempBuf, &data_length)) < 0)
		goto read_rbu_data_exit;

	if ((pos + count) > rbu_data.imagesize) {
		rbu_data.packet_read_count = 0;
		 
		retval = bytes_left;
	} else
		retval = count;

      read_rbu_data_exit:
	return retval;
}

static ssize_t read_rbu_mono_data(char *buffer, loff_t pos, size_t count)
{
	 
	if ((rbu_data.image_update_buffer == NULL) ||
		(rbu_data.bios_image_size == 0)) {
		pr_debug("image_update_buffer %p, bios_image_size %lu\n",
			rbu_data.image_update_buffer,
			rbu_data.bios_image_size);
		return -ENOMEM;
	}

	return memory_read_from_buffer(buffer, count, &pos,
			rbu_data.image_update_buffer, rbu_data.bios_image_size);
}

static ssize_t data_read(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *bin_attr,
			 char *buffer, loff_t pos, size_t count)
{
	ssize_t ret_count = 0;

	spin_lock(&rbu_data.lock);

	if (!strcmp(image_type, "mono"))
		ret_count = read_rbu_mono_data(buffer, pos, count);
	else if (!strcmp(image_type, "packet"))
		ret_count = read_packet_data(buffer, pos, count);
	else
		pr_debug("invalid image type specified\n");

	spin_unlock(&rbu_data.lock);
	return ret_count;
}
static BIN_ATTR_RO(data, 0);

static void callbackfn_rbu(const struct firmware *fw, void *context)
{
	rbu_data.entry_created = 0;

	if (!fw)
		return;

	if (!fw->size)
		goto out;

	spin_lock(&rbu_data.lock);
	if (!strcmp(image_type, "mono")) {
		if (!img_update_realloc(fw->size))
			memcpy(rbu_data.image_update_buffer,
				fw->data, fw->size);
	} else if (!strcmp(image_type, "packet")) {
		 
		packet_empty_list();
		if (packetize_data(fw->data, fw->size))
			 
			packet_empty_list();
	} else
		pr_debug("invalid image type specified\n");
	spin_unlock(&rbu_data.lock);
 out:
	release_firmware(fw);
}

static ssize_t image_type_read(struct file *filp, struct kobject *kobj,
			       struct bin_attribute *bin_attr,
			       char *buffer, loff_t pos, size_t count)
{
	int size = 0;
	if (!pos)
		size = scnprintf(buffer, count, "%s\n", image_type);
	return size;
}

static ssize_t image_type_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buffer, loff_t pos, size_t count)
{
	int rc = count;
	int req_firm_rc = 0;
	int i;
	spin_lock(&rbu_data.lock);
	 
	for (i = 0; i < count; ++i)
		if (buffer[i] == '\n' || buffer[i] == ' ') {
			buffer[i] = '\0';
			break;
		}
	if (i == count)
		buffer[count] = '\0';

	if (strstr(buffer, "mono"))
		strcpy(image_type, "mono");
	else if (strstr(buffer, "packet"))
		strcpy(image_type, "packet");
	else if (strstr(buffer, "init")) {
		 
		if (!rbu_data.entry_created) {
			spin_unlock(&rbu_data.lock);
			req_firm_rc = request_firmware_nowait(THIS_MODULE,
				FW_ACTION_NOUEVENT, "dell_rbu",
				&rbu_device->dev, GFP_KERNEL, &context,
				callbackfn_rbu);
			if (req_firm_rc) {
				pr_err("request_firmware_nowait failed %d\n", rc);
				rc = -EIO;
			} else
				rbu_data.entry_created = 1;

			spin_lock(&rbu_data.lock);
		}
	} else {
		pr_warn("image_type is invalid\n");
		spin_unlock(&rbu_data.lock);
		return -EINVAL;
	}

	 
	packet_empty_list();
	img_update_free();
	spin_unlock(&rbu_data.lock);

	return rc;
}
static BIN_ATTR_RW(image_type, 0);

static ssize_t packet_size_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buffer, loff_t pos, size_t count)
{
	int size = 0;
	if (!pos) {
		spin_lock(&rbu_data.lock);
		size = scnprintf(buffer, count, "%lu\n", rbu_data.packetsize);
		spin_unlock(&rbu_data.lock);
	}
	return size;
}

static ssize_t packet_size_write(struct file *filp, struct kobject *kobj,
				 struct bin_attribute *bin_attr,
				 char *buffer, loff_t pos, size_t count)
{
	unsigned long temp;
	spin_lock(&rbu_data.lock);
	packet_empty_list();
	sscanf(buffer, "%lu", &temp);
	if (temp < 0xffffffff)
		rbu_data.packetsize = temp;

	spin_unlock(&rbu_data.lock);
	return count;
}
static BIN_ATTR_RW(packet_size, 0);

static struct bin_attribute *rbu_bin_attrs[] = {
	&bin_attr_data,
	&bin_attr_image_type,
	&bin_attr_packet_size,
	NULL
};

static const struct attribute_group rbu_group = {
	.bin_attrs = rbu_bin_attrs,
};

static int __init dcdrbu_init(void)
{
	int rc;
	spin_lock_init(&rbu_data.lock);

	init_packet_head();
	rbu_device = platform_device_register_simple("dell_rbu", PLATFORM_DEVID_NONE, NULL, 0);
	if (IS_ERR(rbu_device)) {
		pr_err("platform_device_register_simple failed\n");
		return PTR_ERR(rbu_device);
	}

	rc = sysfs_create_group(&rbu_device->dev.kobj, &rbu_group);
	if (rc)
		goto out_devreg;

	rbu_data.entry_created = 0;
	return 0;

out_devreg:
	platform_device_unregister(rbu_device);
	return rc;
}

static __exit void dcdrbu_exit(void)
{
	spin_lock(&rbu_data.lock);
	packet_empty_list();
	img_update_free();
	spin_unlock(&rbu_data.lock);
	sysfs_remove_group(&rbu_device->dev.kobj, &rbu_group);
	platform_device_unregister(rbu_device);
}

module_exit(dcdrbu_exit);
module_init(dcdrbu_init);
