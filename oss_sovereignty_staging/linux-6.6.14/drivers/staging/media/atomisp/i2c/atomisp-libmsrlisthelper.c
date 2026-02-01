
 
#include <linux/i2c.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/export.h>
#include "../include/linux/libmsrlisthelper.h"
#include <linux/module.h>
#include <linux/slab.h>

 
struct tbd_header {
	u32 tag;           
	u32 size;          
	u32 version;       
	u32 revision;      
	u32 config_bits;   
	u32 checksum;      
} __packed;

struct tbd_record_header {
	u32 size;         
	u8 format_id;     
	u8 packing_key;   
	u16 class_id;     
} __packed;

struct tbd_data_record_header {
	u16 next_offset;
	u16 flags;
	u16 data_offset;
	u16 data_size;
} __packed;

#define TBD_CLASS_DRV_ID 2

static int set_msr_configuration(struct i2c_client *client, uint8_t *bufptr,
				 unsigned int size)
{
	 

	u8 *ptr = bufptr;

	while (ptr < bufptr + size) {
		struct i2c_msg msg = {
			.addr = client->addr,
			.flags = 0,
		};
		int ret;

		 
		msg.len = *ptr++;
		 
		msg.buf = ptr;
		ptr += msg.len;

		if (ptr > bufptr + size)
			 
			return -EINVAL;

		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret < 0) {
			dev_err(&client->dev, "i2c write error: %d", ret);
			return ret;
		}
	}
	return 0;
}

static int parse_and_apply(struct i2c_client *client, uint8_t *buffer,
			   unsigned int size)
{
	u8 *endptr8 = buffer + size;
	struct tbd_data_record_header *header =
	    (struct tbd_data_record_header *)buffer;

	 
	unsigned int dataset = 0;

	do {
		 
		if ((uint8_t *)header + sizeof(*header) > endptr8)
			return -EINVAL;

		 
		if ((uint8_t *)header + header->data_offset +
		    header->data_size > endptr8)
			return -EINVAL;

		 
		dataset++;
		 
		 
		if (header->data_size && (header->flags & 1)) {
			int ret;

			dev_info(&client->dev,
				 "New MSR data for sensor driver (dataset %02d) size:%d\n",
				 dataset, header->data_size);
			ret = set_msr_configuration(client,
						    buffer + header->data_offset,
						    header->data_size);
			if (ret)
				return ret;
		}
		header = (struct tbd_data_record_header *)(buffer +
			 header->next_offset);
	} while (header->next_offset);

	return 0;
}

int apply_msr_data(struct i2c_client *client, const struct firmware *fw)
{
	struct tbd_header *header;
	struct tbd_record_header *record;

	if (!fw) {
		dev_warn(&client->dev, "Drv data is not loaded.\n");
		return -EINVAL;
	}

	if (sizeof(*header) > fw->size)
		return -EINVAL;

	header = (struct tbd_header *)fw->data;
	 
	if (memcmp(&header->tag, "DRVB", 4))
		return -EINVAL;

	 
	if (header->size != fw->size)
		return -EINVAL;

	if (sizeof(*header) + sizeof(*record) > fw->size)
		return -EINVAL;

	record = (struct tbd_record_header *)(header + 1);
	 
	if (record->class_id != TBD_CLASS_DRV_ID)
		return -EINVAL;

	 
	if (!record->size)
		return 0;

	return parse_and_apply(client, (uint8_t *)(record + 1), record->size);
}
EXPORT_SYMBOL_GPL(apply_msr_data);

int load_msr_list(struct i2c_client *client, char *name,
		  const struct firmware **fw)
{
	int ret = request_firmware(fw, name, &client->dev);

	if (ret) {
		dev_err(&client->dev,
			"Error %d while requesting firmware %s\n",
			ret, name);
		return ret;
	}
	dev_info(&client->dev, "Received %lu bytes drv data\n",
		 (unsigned long)(*fw)->size);

	return 0;
}
EXPORT_SYMBOL_GPL(load_msr_list);

void release_msr_list(struct i2c_client *client, const struct firmware *fw)
{
	release_firmware(fw);
}
EXPORT_SYMBOL_GPL(release_msr_list);

static int init_msrlisthelper(void)
{
	return 0;
}

static void exit_msrlisthelper(void)
{
}

module_init(init_msrlisthelper);
module_exit(exit_msrlisthelper);

MODULE_AUTHOR("Jukka Kaartinen <jukka.o.kaartinen@intel.com>");
MODULE_LICENSE("GPL");
