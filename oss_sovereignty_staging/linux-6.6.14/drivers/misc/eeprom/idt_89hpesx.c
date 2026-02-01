
 
 

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/i2c.h>
#include <linux/pci_ids.h>
#include <linux/delay.h>

#define IDT_NAME		"89hpesx"
#define IDT_89HPESX_DESC	"IDT 89HPESx SMBus-slave interface driver"
#define IDT_89HPESX_VER		"1.0"

MODULE_DESCRIPTION(IDT_89HPESX_DESC);
MODULE_VERSION(IDT_89HPESX_VER);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("T-platforms");

 
static struct dentry *csr_dbgdir;

 
struct idt_smb_seq;
struct idt_89hpesx_dev {
	u32 eesize;
	bool eero;
	u8 eeaddr;

	u8 inieecmd;
	u8 inicsrcmd;
	u8 iniccode;

	u16 csr;

	int (*smb_write)(struct idt_89hpesx_dev *, const struct idt_smb_seq *);
	int (*smb_read)(struct idt_89hpesx_dev *, struct idt_smb_seq *);
	struct mutex smb_mtx;

	struct i2c_client *client;

	struct bin_attribute *ee_file;
	struct dentry *csr_dir;
};

 
struct idt_smb_seq {
	u8 ccode;
	u8 bytecnt;
	u8 *data;
};

 
struct idt_eeprom_seq {
	u8 cmd;
	u8 eeaddr;
	u16 memaddr;
	u8 data;
} __packed;

 
struct idt_csr_seq {
	u8 cmd;
	u16 csraddr;
	u32 data;
} __packed;

 
#define CCODE_END	((u8)0x01)
#define CCODE_START	((u8)0x02)
#define CCODE_CSR	((u8)0x00)
#define CCODE_EEPROM	((u8)0x04)
#define CCODE_BYTE	((u8)0x00)
#define CCODE_WORD	((u8)0x20)
#define CCODE_BLOCK	((u8)0x40)
#define CCODE_PEC	((u8)0x80)

 
#define EEPROM_OP_WRITE	((u8)0x00)
#define EEPROM_OP_READ	((u8)0x01)
#define EEPROM_USA	((u8)0x02)
#define EEPROM_NAERR	((u8)0x08)
#define EEPROM_LAERR    ((u8)0x10)
#define EEPROM_MSS	((u8)0x20)
#define EEPROM_WR_CNT	((u8)5)
#define EEPROM_WRRD_CNT	((u8)4)
#define EEPROM_RD_CNT	((u8)5)
#define EEPROM_DEF_SIZE	((u16)4096)
#define EEPROM_DEF_ADDR	((u8)0x50)
#define EEPROM_TOUT	(100)

 
#define CSR_DWE			((u8)0x0F)
#define CSR_OP_WRITE		((u8)0x00)
#define CSR_OP_READ		((u8)0x10)
#define CSR_RERR		((u8)0x40)
#define CSR_WERR		((u8)0x80)
#define CSR_WR_CNT		((u8)7)
#define CSR_WRRD_CNT		((u8)3)
#define CSR_RD_CNT		((u8)7)
#define CSR_MAX			((u32)0x3FFFF)
#define CSR_DEF			((u16)0x0000)
#define CSR_REAL_ADDR(val)	((unsigned int)val << 2)

 
#define IDT_VIDDID_CSR	((u32)0x0000)
#define IDT_VID_MASK	((u32)0xFFFF)

 
#define RETRY_CNT (128)
#define idt_smb_safe(ops, args...) ({ \
	int __retry = RETRY_CNT; \
	s32 __sts; \
	do { \
		__sts = i2c_smbus_ ## ops ## _data(args); \
	} while (__retry-- && __sts < 0); \
	__sts; \
})

 

 
static int idt_smb_write_byte(struct idt_89hpesx_dev *pdev,
			      const struct idt_smb_seq *seq)
{
	s32 sts;
	u8 ccode;
	int idx;

	 
	for (idx = 0; idx < seq->bytecnt; idx++) {
		 
		ccode = seq->ccode | CCODE_BYTE;
		if (idx == 0)
			ccode |= CCODE_START;
		if (idx == seq->bytecnt - 1)
			ccode |= CCODE_END;

		 
		sts = idt_smb_safe(write_byte, pdev->client, ccode,
			seq->data[idx]);
		if (sts != 0)
			return (int)sts;
	}

	return 0;
}

 
static int idt_smb_read_byte(struct idt_89hpesx_dev *pdev,
			     struct idt_smb_seq *seq)
{
	s32 sts;
	u8 ccode;
	int idx;

	 
	for (idx = 0; idx < seq->bytecnt; idx++) {
		 
		ccode = seq->ccode | CCODE_BYTE;
		if (idx == 0)
			ccode |= CCODE_START;
		if (idx == seq->bytecnt - 1)
			ccode |= CCODE_END;

		 
		sts = idt_smb_safe(read_byte, pdev->client, ccode);
		if (sts < 0)
			return (int)sts;

		seq->data[idx] = (u8)sts;
	}

	return 0;
}

 
static int idt_smb_write_word(struct idt_89hpesx_dev *pdev,
			      const struct idt_smb_seq *seq)
{
	s32 sts;
	u8 ccode;
	int idx, evencnt;

	 
	evencnt = seq->bytecnt - (seq->bytecnt % 2);

	 
	for (idx = 0; idx < evencnt; idx += 2) {
		 
		ccode = seq->ccode | CCODE_WORD;
		if (idx == 0)
			ccode |= CCODE_START;
		if (idx == evencnt - 2)
			ccode |= CCODE_END;

		 
		sts = idt_smb_safe(write_word, pdev->client, ccode,
			*(u16 *)&seq->data[idx]);
		if (sts != 0)
			return (int)sts;
	}

	 
	if (seq->bytecnt != evencnt) {
		 
		ccode = seq->ccode | CCODE_BYTE | CCODE_END;
		if (idx == 0)
			ccode |= CCODE_START;

		 
		sts = idt_smb_safe(write_byte, pdev->client, ccode,
			seq->data[idx]);
		if (sts != 0)
			return (int)sts;
	}

	return 0;
}

 
static int idt_smb_read_word(struct idt_89hpesx_dev *pdev,
			     struct idt_smb_seq *seq)
{
	s32 sts;
	u8 ccode;
	int idx, evencnt;

	 
	evencnt = seq->bytecnt - (seq->bytecnt % 2);

	 
	for (idx = 0; idx < evencnt; idx += 2) {
		 
		ccode = seq->ccode | CCODE_WORD;
		if (idx == 0)
			ccode |= CCODE_START;
		if (idx == evencnt - 2)
			ccode |= CCODE_END;

		 
		sts = idt_smb_safe(read_word, pdev->client, ccode);
		if (sts < 0)
			return (int)sts;

		*(u16 *)&seq->data[idx] = (u16)sts;
	}

	 
	if (seq->bytecnt != evencnt) {
		 
		ccode = seq->ccode | CCODE_BYTE | CCODE_END;
		if (idx == 0)
			ccode |= CCODE_START;

		 
		sts = idt_smb_safe(read_byte, pdev->client, ccode);
		if (sts < 0)
			return (int)sts;

		seq->data[idx] = (u8)sts;
	}

	return 0;
}

 
static int idt_smb_write_block(struct idt_89hpesx_dev *pdev,
			       const struct idt_smb_seq *seq)
{
	u8 ccode;

	 
	if (seq->bytecnt > I2C_SMBUS_BLOCK_MAX)
		return -EINVAL;

	 
	ccode = seq->ccode | CCODE_BLOCK | CCODE_START | CCODE_END;

	 
	return idt_smb_safe(write_block, pdev->client, ccode, seq->bytecnt,
		seq->data);
}

 
static int idt_smb_read_block(struct idt_89hpesx_dev *pdev,
			      struct idt_smb_seq *seq)
{
	s32 sts;
	u8 ccode;

	 
	if (seq->bytecnt > I2C_SMBUS_BLOCK_MAX)
		return -EINVAL;

	 
	ccode = seq->ccode | CCODE_BLOCK | CCODE_START | CCODE_END;

	 
	sts = idt_smb_safe(read_block, pdev->client, ccode, seq->data);
	if (sts != seq->bytecnt)
		return (sts < 0 ? sts : -ENODATA);

	return 0;
}

 
static int idt_smb_write_i2c_block(struct idt_89hpesx_dev *pdev,
				   const struct idt_smb_seq *seq)
{
	u8 ccode, buf[I2C_SMBUS_BLOCK_MAX + 1];

	 
	if (seq->bytecnt > I2C_SMBUS_BLOCK_MAX)
		return -EINVAL;

	 
	buf[0] = seq->bytecnt;
	memcpy(&buf[1], seq->data, seq->bytecnt);

	 
	ccode = seq->ccode | CCODE_BLOCK | CCODE_START | CCODE_END;

	 
	return idt_smb_safe(write_i2c_block, pdev->client, ccode,
		seq->bytecnt + 1, buf);
}

 
static int idt_smb_read_i2c_block(struct idt_89hpesx_dev *pdev,
				  struct idt_smb_seq *seq)
{
	u8 ccode, buf[I2C_SMBUS_BLOCK_MAX + 1];
	s32 sts;

	 
	if (seq->bytecnt > I2C_SMBUS_BLOCK_MAX)
		return -EINVAL;

	 
	ccode = seq->ccode | CCODE_BLOCK | CCODE_START | CCODE_END;

	 
	sts = idt_smb_safe(read_i2c_block, pdev->client, ccode,
		seq->bytecnt + 1, buf);
	if (sts != seq->bytecnt + 1)
		return (sts < 0 ? sts : -ENODATA);
	if (buf[0] != seq->bytecnt)
		return -ENODATA;

	 
	memcpy(seq->data, &buf[1], seq->bytecnt);

	return 0;
}

 

 
static int idt_eeprom_read_byte(struct idt_89hpesx_dev *pdev, u16 memaddr,
				u8 *data)
{
	struct device *dev = &pdev->client->dev;
	struct idt_eeprom_seq eeseq;
	struct idt_smb_seq smbseq;
	int ret, retry;

	 
	smbseq.ccode = pdev->iniccode | CCODE_EEPROM;
	smbseq.data = (u8 *)&eeseq;

	 
	retry = RETRY_CNT;
	do {
		 
		smbseq.bytecnt = EEPROM_WRRD_CNT;
		eeseq.cmd = pdev->inieecmd | EEPROM_OP_READ;
		eeseq.eeaddr = pdev->eeaddr;
		eeseq.memaddr = cpu_to_le16(memaddr);
		ret = pdev->smb_write(pdev, &smbseq);
		if (ret != 0) {
			dev_err(dev, "Failed to init eeprom addr 0x%02x",
				memaddr);
			break;
		}

		 
		smbseq.bytecnt = EEPROM_RD_CNT;
		ret = pdev->smb_read(pdev, &smbseq);
		if (ret != 0) {
			dev_err(dev, "Failed to read eeprom data 0x%02x",
				memaddr);
			break;
		}

		 
		if (retry && (eeseq.cmd & EEPROM_NAERR)) {
			dev_dbg(dev, "EEPROM busy, retry reading after %d ms",
				EEPROM_TOUT);
			msleep(EEPROM_TOUT);
			continue;
		}

		 
		if (eeseq.cmd & (EEPROM_NAERR | EEPROM_LAERR | EEPROM_MSS)) {
			dev_err(dev,
				"Communication with eeprom failed, cmd 0x%hhx",
				eeseq.cmd);
			ret = -EREMOTEIO;
			break;
		}

		 
		*data = eeseq.data;
		break;
	} while (retry--);

	 
	return ret;
}

 
static int idt_eeprom_write(struct idt_89hpesx_dev *pdev, u16 memaddr, u16 len,
			    const u8 *data)
{
	struct device *dev = &pdev->client->dev;
	struct idt_eeprom_seq eeseq;
	struct idt_smb_seq smbseq;
	int ret;
	u16 idx;

	 
	smbseq.ccode = pdev->iniccode | CCODE_EEPROM;
	smbseq.data = (u8 *)&eeseq;

	 
	for (idx = 0; idx < len; idx++, memaddr++) {
		 
		mutex_lock(&pdev->smb_mtx);

		 
		smbseq.bytecnt = EEPROM_WR_CNT;
		eeseq.cmd = pdev->inieecmd | EEPROM_OP_WRITE;
		eeseq.eeaddr = pdev->eeaddr;
		eeseq.memaddr = cpu_to_le16(memaddr);
		eeseq.data = data[idx];
		ret = pdev->smb_write(pdev, &smbseq);
		if (ret != 0) {
			dev_err(dev,
				"Failed to write 0x%04hx:0x%02hhx to eeprom",
				memaddr, data[idx]);
			goto err_mutex_unlock;
		}

		 
		eeseq.data = ~data[idx];
		ret = idt_eeprom_read_byte(pdev, memaddr, &eeseq.data);
		if (ret != 0)
			goto err_mutex_unlock;

		 
		if (eeseq.data != data[idx]) {
			dev_err(dev, "Values don't match 0x%02hhx != 0x%02hhx",
				eeseq.data, data[idx]);
			ret = -EREMOTEIO;
			goto err_mutex_unlock;
		}

		 
err_mutex_unlock:
		mutex_unlock(&pdev->smb_mtx);
		if (ret != 0)
			return ret;
	}

	return 0;
}

 
static int idt_eeprom_read(struct idt_89hpesx_dev *pdev, u16 memaddr, u16 len,
			   u8 *buf)
{
	int ret;
	u16 idx;

	 
	for (idx = 0; idx < len; idx++, memaddr++) {
		 
		mutex_lock(&pdev->smb_mtx);

		 
		ret = idt_eeprom_read_byte(pdev, memaddr, &buf[idx]);

		 
		mutex_unlock(&pdev->smb_mtx);

		 
		if (ret != 0)
			return ret;
	}

	return 0;
}

 

 
static int idt_csr_write(struct idt_89hpesx_dev *pdev, u16 csraddr,
			 const u32 data)
{
	struct device *dev = &pdev->client->dev;
	struct idt_csr_seq csrseq;
	struct idt_smb_seq smbseq;
	int ret;

	 
	smbseq.ccode = pdev->iniccode | CCODE_CSR;
	smbseq.data = (u8 *)&csrseq;

	 
	mutex_lock(&pdev->smb_mtx);

	 
	smbseq.bytecnt = CSR_WR_CNT;
	csrseq.cmd = pdev->inicsrcmd | CSR_OP_WRITE;
	csrseq.csraddr = cpu_to_le16(csraddr);
	csrseq.data = cpu_to_le32(data);
	ret = pdev->smb_write(pdev, &smbseq);
	if (ret != 0) {
		dev_err(dev, "Failed to write 0x%04x: 0x%04x to csr",
			CSR_REAL_ADDR(csraddr), data);
		goto err_mutex_unlock;
	}

	 
	smbseq.bytecnt = CSR_WRRD_CNT;
	csrseq.cmd = pdev->inicsrcmd | CSR_OP_READ;
	ret = pdev->smb_write(pdev, &smbseq);
	if (ret != 0) {
		dev_err(dev, "Failed to init csr address 0x%04x",
			CSR_REAL_ADDR(csraddr));
		goto err_mutex_unlock;
	}

	 
	smbseq.bytecnt = CSR_RD_CNT;
	ret = pdev->smb_read(pdev, &smbseq);
	if (ret != 0) {
		dev_err(dev, "Failed to read csr 0x%04x",
			CSR_REAL_ADDR(csraddr));
		goto err_mutex_unlock;
	}

	 
	if (csrseq.cmd & (CSR_RERR | CSR_WERR)) {
		dev_err(dev, "IDT failed to perform CSR r/w");
		ret = -EREMOTEIO;
		goto err_mutex_unlock;
	}

	 
err_mutex_unlock:
	mutex_unlock(&pdev->smb_mtx);

	return ret;
}

 
static int idt_csr_read(struct idt_89hpesx_dev *pdev, u16 csraddr, u32 *data)
{
	struct device *dev = &pdev->client->dev;
	struct idt_csr_seq csrseq;
	struct idt_smb_seq smbseq;
	int ret;

	 
	smbseq.ccode = pdev->iniccode | CCODE_CSR;
	smbseq.data = (u8 *)&csrseq;

	 
	mutex_lock(&pdev->smb_mtx);

	 
	smbseq.bytecnt = CSR_WRRD_CNT;
	csrseq.cmd = pdev->inicsrcmd | CSR_OP_READ;
	csrseq.csraddr = cpu_to_le16(csraddr);
	ret = pdev->smb_write(pdev, &smbseq);
	if (ret != 0) {
		dev_err(dev, "Failed to init csr address 0x%04x",
			CSR_REAL_ADDR(csraddr));
		goto err_mutex_unlock;
	}

	 
	smbseq.bytecnt = CSR_RD_CNT;
	ret = pdev->smb_read(pdev, &smbseq);
	if (ret != 0) {
		dev_err(dev, "Failed to read csr 0x%04x",
			CSR_REAL_ADDR(csraddr));
		goto err_mutex_unlock;
	}

	 
	if (csrseq.cmd & (CSR_RERR | CSR_WERR)) {
		dev_err(dev, "IDT failed to perform CSR r/w");
		ret = -EREMOTEIO;
		goto err_mutex_unlock;
	}

	 
	*data = le32_to_cpu(csrseq.data);

	 
err_mutex_unlock:
	mutex_unlock(&pdev->smb_mtx);

	return ret;
}

 

 
static ssize_t eeprom_write(struct file *filp, struct kobject *kobj,
			    struct bin_attribute *attr,
			    char *buf, loff_t off, size_t count)
{
	struct idt_89hpesx_dev *pdev;
	int ret;

	 
	pdev = dev_get_drvdata(kobj_to_dev(kobj));

	 
	ret = idt_eeprom_write(pdev, (u16)off, (u16)count, (u8 *)buf);
	return (ret != 0 ? ret : count);
}

 
static ssize_t eeprom_read(struct file *filp, struct kobject *kobj,
			   struct bin_attribute *attr,
			   char *buf, loff_t off, size_t count)
{
	struct idt_89hpesx_dev *pdev;
	int ret;

	 
	pdev = dev_get_drvdata(kobj_to_dev(kobj));

	 
	ret = idt_eeprom_read(pdev, (u16)off, (u16)count, (u8 *)buf);
	return (ret != 0 ? ret : count);
}

 
static ssize_t idt_dbgfs_csr_write(struct file *filep, const char __user *ubuf,
				   size_t count, loff_t *offp)
{
	struct idt_89hpesx_dev *pdev = filep->private_data;
	char *colon_ch, *csraddr_str, *csrval_str;
	int ret, csraddr_len;
	u32 csraddr, csrval;
	char *buf;

	if (*offp)
		return 0;

	 
	buf = memdup_user_nul(ubuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	 
	colon_ch = strnchr(buf, count, ':');

	 
	if (colon_ch != NULL) {
		csraddr_len = colon_ch - buf;
		csraddr_str =
			kmalloc(csraddr_len + 1, GFP_KERNEL);
		if (csraddr_str == NULL) {
			ret = -ENOMEM;
			goto free_buf;
		}
		 
		strncpy(csraddr_str, buf, csraddr_len);
		csraddr_str[csraddr_len] = '\0';
		 
		csrval_str = colon_ch + 1;
	} else   {
		csraddr_str = (char *)buf;  
		csraddr_len = strnlen(csraddr_str, count);
		csrval_str = NULL;
	}

	 
	ret = kstrtou32(csraddr_str, 0, &csraddr);
	if (ret != 0)
		goto free_csraddr_str;

	 
	if (csraddr > CSR_MAX || !IS_ALIGNED(csraddr, SZ_4)) {
		ret = -EINVAL;
		goto free_csraddr_str;
	}

	 
	pdev->csr = (csraddr >> 2);

	 
	if (colon_ch != NULL) {
		ret = kstrtou32(csrval_str, 0, &csrval);
		if (ret != 0)
			goto free_csraddr_str;

		ret = idt_csr_write(pdev, pdev->csr, csrval);
		if (ret != 0)
			goto free_csraddr_str;
	}

	 
free_csraddr_str:
	if (colon_ch != NULL)
		kfree(csraddr_str);

	 
free_buf:
	kfree(buf);

	return (ret != 0 ? ret : count);
}

 
#define CSRBUF_SIZE	((size_t)32)
static ssize_t idt_dbgfs_csr_read(struct file *filep, char __user *ubuf,
				  size_t count, loff_t *offp)
{
	struct idt_89hpesx_dev *pdev = filep->private_data;
	u32 csraddr, csrval;
	char buf[CSRBUF_SIZE];
	int ret, size;

	 
	ret = idt_csr_read(pdev, pdev->csr, &csrval);
	if (ret != 0)
		return ret;

	 
	csraddr = ((u32)pdev->csr << 2);

	 
	size = snprintf(buf, CSRBUF_SIZE, "0x%05x:0x%08x\n",
		(unsigned int)csraddr, (unsigned int)csrval);

	 
	return simple_read_from_buffer(ubuf, count, offp, buf, size);
}

 
static BIN_ATTR_RW(eeprom, EEPROM_DEF_SIZE);

 
static const struct file_operations csr_dbgfs_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = idt_dbgfs_csr_write,
	.read = idt_dbgfs_csr_read
};

 

 
static void idt_set_defval(struct idt_89hpesx_dev *pdev)
{
	 
	pdev->eesize = 0;
	pdev->eero = true;
	pdev->inieecmd = 0;
	pdev->eeaddr = 0;
}

static const struct i2c_device_id ee_ids[];

 
static const struct i2c_device_id *idt_ee_match_id(struct fwnode_handle *fwnode)
{
	const struct i2c_device_id *id = ee_ids;
	const char *compatible, *p;
	char devname[I2C_NAME_SIZE];
	int ret;

	ret = fwnode_property_read_string(fwnode, "compatible", &compatible);
	if (ret)
		return NULL;

	p = strchr(compatible, ',');
	strscpy(devname, p ? p + 1 : compatible, sizeof(devname));
	 
	while (id->name[0]) {
		if (strcmp(devname, id->name) == 0)
			return id;
		id++;
	}
	return NULL;
}

 
static void idt_get_fw_data(struct idt_89hpesx_dev *pdev)
{
	struct device *dev = &pdev->client->dev;
	struct fwnode_handle *fwnode;
	const struct i2c_device_id *ee_id = NULL;
	u32 eeprom_addr;
	int ret;

	device_for_each_child_node(dev, fwnode) {
		ee_id = idt_ee_match_id(fwnode);
		if (ee_id)
			break;

		dev_warn(dev, "Skip unsupported EEPROM device %pfw\n", fwnode);
	}

	 
	if (!ee_id) {
		dev_warn(dev, "No fwnode, EEPROM access disabled");
		idt_set_defval(pdev);
		return;
	}

	 
	pdev->eesize = (u32)ee_id->driver_data;

	 
	ret = fwnode_property_read_u32(fwnode, "reg", &eeprom_addr);
	if (ret || (eeprom_addr == 0)) {
		dev_warn(dev, "No EEPROM reg found, use default address 0x%x",
			 EEPROM_DEF_ADDR);
		pdev->inieecmd = 0;
		pdev->eeaddr = EEPROM_DEF_ADDR << 1;
	} else {
		pdev->inieecmd = EEPROM_USA;
		pdev->eeaddr = eeprom_addr << 1;
	}

	 
	if (fwnode_property_read_bool(fwnode, "read-only"))
		pdev->eero = true;
	else  
		pdev->eero = false;

	fwnode_handle_put(fwnode);
	dev_info(dev, "EEPROM of %d bytes found by 0x%x",
		pdev->eesize, pdev->eeaddr);
}

 
static struct idt_89hpesx_dev *idt_create_pdev(struct i2c_client *client)
{
	struct idt_89hpesx_dev *pdev;

	 
	pdev = devm_kmalloc(&client->dev, sizeof(struct idt_89hpesx_dev),
		GFP_KERNEL);
	if (pdev == NULL)
		return ERR_PTR(-ENOMEM);

	 
	pdev->client = client;
	i2c_set_clientdata(client, pdev);

	 
	idt_get_fw_data(pdev);

	 
	pdev->inicsrcmd = CSR_DWE;
	pdev->csr = CSR_DEF;

	 
	if (i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_PEC)) {
		pdev->iniccode = CCODE_PEC;
		client->flags |= I2C_CLIENT_PEC;
	} else   {
		pdev->iniccode = 0;
	}

	return pdev;
}

 
static void idt_free_pdev(struct idt_89hpesx_dev *pdev)
{
	 
	i2c_set_clientdata(pdev->client, NULL);
}

 
static int idt_set_smbus_ops(struct idt_89hpesx_dev *pdev)
{
	struct i2c_adapter *adapter = pdev->client->adapter;
	struct device *dev = &pdev->client->dev;

	 
	if (i2c_check_functionality(adapter,
				    I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
		pdev->smb_read = idt_smb_read_block;
		dev_dbg(dev, "SMBus block-read op chosen");
	} else if (i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
		pdev->smb_read = idt_smb_read_i2c_block;
		dev_dbg(dev, "SMBus i2c-block-read op chosen");
	} else if (i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_READ_WORD_DATA) &&
		   i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
		pdev->smb_read = idt_smb_read_word;
		dev_warn(dev, "Use slow word/byte SMBus read ops");
	} else if (i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
		pdev->smb_read = idt_smb_read_byte;
		dev_warn(dev, "Use slow byte SMBus read op");
	} else   {
		dev_err(dev, "No supported SMBus read op");
		return -EPFNOSUPPORT;
	}

	 
	if (i2c_check_functionality(adapter,
				    I2C_FUNC_SMBUS_WRITE_BLOCK_DATA)) {
		pdev->smb_write = idt_smb_write_block;
		dev_dbg(dev, "SMBus block-write op chosen");
	} else if (i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {
		pdev->smb_write = idt_smb_write_i2c_block;
		dev_dbg(dev, "SMBus i2c-block-write op chosen");
	} else if (i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_WRITE_WORD_DATA) &&
		   i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		pdev->smb_write = idt_smb_write_word;
		dev_warn(dev, "Use slow word/byte SMBus write op");
	} else if (i2c_check_functionality(adapter,
					   I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		pdev->smb_write = idt_smb_write_byte;
		dev_warn(dev, "Use slow byte SMBus write op");
	} else   {
		dev_err(dev, "No supported SMBus write op");
		return -EPFNOSUPPORT;
	}

	 
	mutex_init(&pdev->smb_mtx);

	return 0;
}

 
static int idt_check_dev(struct idt_89hpesx_dev *pdev)
{
	struct device *dev = &pdev->client->dev;
	u32 viddid;
	int ret;

	 
	ret = idt_csr_read(pdev, IDT_VIDDID_CSR, &viddid);
	if (ret != 0) {
		dev_err(dev, "Failed to read VID/DID");
		return ret;
	}

	 
	if ((viddid & IDT_VID_MASK) != PCI_VENDOR_ID_IDT) {
		dev_err(dev, "Got unsupported VID/DID: 0x%08x", viddid);
		return -ENODEV;
	}

	dev_info(dev, "Found IDT 89HPES device VID:0x%04x, DID:0x%04x",
		(viddid & IDT_VID_MASK), (viddid >> 16));

	return 0;
}

 
static int idt_create_sysfs_files(struct idt_89hpesx_dev *pdev)
{
	struct device *dev = &pdev->client->dev;
	int ret;

	 
	if (pdev->eesize == 0) {
		dev_dbg(dev, "Skip creating sysfs-files");
		return 0;
	}

	 
	pdev->ee_file = devm_kmemdup(dev, &bin_attr_eeprom,
				     sizeof(*pdev->ee_file), GFP_KERNEL);
	if (!pdev->ee_file)
		return -ENOMEM;

	 
	if (pdev->eero) {
		pdev->ee_file->attr.mode &= ~0200;
		pdev->ee_file->write = NULL;
	}
	 
	pdev->ee_file->size = pdev->eesize;
	ret = sysfs_create_bin_file(&dev->kobj, pdev->ee_file);
	if (ret != 0) {
		dev_err(dev, "Failed to create EEPROM sysfs-node");
		return ret;
	}

	return 0;
}

 
static void idt_remove_sysfs_files(struct idt_89hpesx_dev *pdev)
{
	struct device *dev = &pdev->client->dev;

	 
	if (pdev->eesize == 0)
		return;

	 
	sysfs_remove_bin_file(&dev->kobj, pdev->ee_file);
}

 
#define CSRNAME_LEN	((size_t)32)
static void idt_create_dbgfs_files(struct idt_89hpesx_dev *pdev)
{
	struct i2c_client *cli = pdev->client;
	char fname[CSRNAME_LEN];

	 
	snprintf(fname, CSRNAME_LEN, "%d-%04hx", cli->adapter->nr, cli->addr);
	pdev->csr_dir = debugfs_create_dir(fname, csr_dbgdir);

	 
	debugfs_create_file(cli->name, 0600, pdev->csr_dir, pdev,
			    &csr_dbgfs_ops);
}

 
static void idt_remove_dbgfs_files(struct idt_89hpesx_dev *pdev)
{
	 
	debugfs_remove_recursive(pdev->csr_dir);
}

 
static int idt_probe(struct i2c_client *client)
{
	struct idt_89hpesx_dev *pdev;
	int ret;

	 
	pdev = idt_create_pdev(client);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	 
	ret = idt_set_smbus_ops(pdev);
	if (ret != 0)
		goto err_free_pdev;

	 
	ret = idt_check_dev(pdev);
	if (ret != 0)
		goto err_free_pdev;

	 
	ret = idt_create_sysfs_files(pdev);
	if (ret != 0)
		goto err_free_pdev;

	 
	idt_create_dbgfs_files(pdev);

	return 0;

err_free_pdev:
	idt_free_pdev(pdev);

	return ret;
}

 
static void idt_remove(struct i2c_client *client)
{
	struct idt_89hpesx_dev *pdev = i2c_get_clientdata(client);

	 
	idt_remove_dbgfs_files(pdev);

	 
	idt_remove_sysfs_files(pdev);

	 
	idt_free_pdev(pdev);
}

 
static const struct i2c_device_id ee_ids[] = {
	{ "24c32",  4096},
	{ "24c64",  8192},
	{ "24c128", 16384},
	{ "24c256", 32768},
	{ "24c512", 65536},
	{}
};
MODULE_DEVICE_TABLE(i2c, ee_ids);

 
static const struct i2c_device_id idt_ids[] = {
	{ "89hpes8nt2", 0 },
	{ "89hpes12nt3", 0 },

	{ "89hpes24nt6ag2", 0 },
	{ "89hpes32nt8ag2", 0 },
	{ "89hpes32nt8bg2", 0 },
	{ "89hpes12nt12g2", 0 },
	{ "89hpes16nt16g2", 0 },
	{ "89hpes24nt24g2", 0 },
	{ "89hpes32nt24ag2", 0 },
	{ "89hpes32nt24bg2", 0 },

	{ "89hpes12n3", 0 },
	{ "89hpes12n3a", 0 },
	{ "89hpes24n3", 0 },
	{ "89hpes24n3a", 0 },

	{ "89hpes32h8", 0 },
	{ "89hpes32h8g2", 0 },
	{ "89hpes48h12", 0 },
	{ "89hpes48h12g2", 0 },
	{ "89hpes48h12ag2", 0 },
	{ "89hpes16h16", 0 },
	{ "89hpes22h16", 0 },
	{ "89hpes22h16g2", 0 },
	{ "89hpes34h16", 0 },
	{ "89hpes34h16g2", 0 },
	{ "89hpes64h16", 0 },
	{ "89hpes64h16g2", 0 },
	{ "89hpes64h16ag2", 0 },

	 
	{ "89hpes12t3g2", 0 },
	{ "89hpes24t3g2", 0 },
	 
	{ "89hpes16t4", 0 },
	{ "89hpes4t4g2", 0 },
	{ "89hpes10t4g2", 0 },
	{ "89hpes16t4g2", 0 },
	{ "89hpes16t4ag2", 0 },
	{ "89hpes5t5", 0 },
	{ "89hpes6t5", 0 },
	{ "89hpes8t5", 0 },
	{ "89hpes8t5a", 0 },
	{ "89hpes24t6", 0 },
	{ "89hpes6t6g2", 0 },
	{ "89hpes24t6g2", 0 },
	{ "89hpes16t7", 0 },
	{ "89hpes32t8", 0 },
	{ "89hpes32t8g2", 0 },
	{ "89hpes48t12", 0 },
	{ "89hpes48t12g2", 0 },
	{   }
};
MODULE_DEVICE_TABLE(i2c, idt_ids);

static const struct of_device_id idt_of_match[] = {
	{ .compatible = "idt,89hpes8nt2", },
	{ .compatible = "idt,89hpes12nt3", },

	{ .compatible = "idt,89hpes24nt6ag2", },
	{ .compatible = "idt,89hpes32nt8ag2", },
	{ .compatible = "idt,89hpes32nt8bg2", },
	{ .compatible = "idt,89hpes12nt12g2", },
	{ .compatible = "idt,89hpes16nt16g2", },
	{ .compatible = "idt,89hpes24nt24g2", },
	{ .compatible = "idt,89hpes32nt24ag2", },
	{ .compatible = "idt,89hpes32nt24bg2", },

	{ .compatible = "idt,89hpes12n3", },
	{ .compatible = "idt,89hpes12n3a", },
	{ .compatible = "idt,89hpes24n3", },
	{ .compatible = "idt,89hpes24n3a", },

	{ .compatible = "idt,89hpes32h8", },
	{ .compatible = "idt,89hpes32h8g2", },
	{ .compatible = "idt,89hpes48h12", },
	{ .compatible = "idt,89hpes48h12g2", },
	{ .compatible = "idt,89hpes48h12ag2", },
	{ .compatible = "idt,89hpes16h16", },
	{ .compatible = "idt,89hpes22h16", },
	{ .compatible = "idt,89hpes22h16g2", },
	{ .compatible = "idt,89hpes34h16", },
	{ .compatible = "idt,89hpes34h16g2", },
	{ .compatible = "idt,89hpes64h16", },
	{ .compatible = "idt,89hpes64h16g2", },
	{ .compatible = "idt,89hpes64h16ag2", },

	{ .compatible = "idt,89hpes12t3g2", },
	{ .compatible = "idt,89hpes24t3g2", },

	{ .compatible = "idt,89hpes16t4", },
	{ .compatible = "idt,89hpes4t4g2", },
	{ .compatible = "idt,89hpes10t4g2", },
	{ .compatible = "idt,89hpes16t4g2", },
	{ .compatible = "idt,89hpes16t4ag2", },
	{ .compatible = "idt,89hpes5t5", },
	{ .compatible = "idt,89hpes6t5", },
	{ .compatible = "idt,89hpes8t5", },
	{ .compatible = "idt,89hpes8t5a", },
	{ .compatible = "idt,89hpes24t6", },
	{ .compatible = "idt,89hpes6t6g2", },
	{ .compatible = "idt,89hpes24t6g2", },
	{ .compatible = "idt,89hpes16t7", },
	{ .compatible = "idt,89hpes32t8", },
	{ .compatible = "idt,89hpes32t8g2", },
	{ .compatible = "idt,89hpes48t12", },
	{ .compatible = "idt,89hpes48t12g2", },
	{ },
};
MODULE_DEVICE_TABLE(of, idt_of_match);

 
static struct i2c_driver idt_driver = {
	.driver = {
		.name = IDT_NAME,
		.of_match_table = idt_of_match,
	},
	.probe = idt_probe,
	.remove = idt_remove,
	.id_table = idt_ids,
};

 
static int __init idt_init(void)
{
	int ret;

	 
	if (debugfs_initialized())
		csr_dbgdir = debugfs_create_dir("idt_csr", NULL);

	 
	ret = i2c_add_driver(&idt_driver);
	if (ret) {
		debugfs_remove_recursive(csr_dbgdir);
		return ret;
	}

	return 0;
}
module_init(idt_init);

 
static void __exit idt_exit(void)
{
	 
	debugfs_remove_recursive(csr_dbgdir);

	 
	i2c_del_driver(&idt_driver);
}
module_exit(idt_exit);
