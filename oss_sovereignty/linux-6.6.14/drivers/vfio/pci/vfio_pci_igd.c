
 

#include <linux/io.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/vfio.h>

#include "vfio_pci_priv.h"

#define OPREGION_SIGNATURE	"IntelGraphicsMem"
#define OPREGION_SIZE		(8 * 1024)
#define OPREGION_PCI_ADDR	0xfc

#define OPREGION_RVDA		0x3ba
#define OPREGION_RVDS		0x3c2
#define OPREGION_VERSION	0x16

struct igd_opregion_vbt {
	void *opregion;
	void *vbt_ex;
};

 
static inline unsigned long igd_opregion_shift_copy(char __user *dst,
						    loff_t *off,
						    void *src,
						    loff_t *pos,
						    size_t *remaining,
						    size_t bytes)
{
	if (copy_to_user(dst + (*off), src, bytes))
		return -EFAULT;

	*off += bytes;
	*pos += bytes;
	*remaining -= bytes;

	return 0;
}

static ssize_t vfio_pci_igd_rw(struct vfio_pci_core_device *vdev,
			       char __user *buf, size_t count, loff_t *ppos,
			       bool iswrite)
{
	unsigned int i = VFIO_PCI_OFFSET_TO_INDEX(*ppos) - VFIO_PCI_NUM_REGIONS;
	struct igd_opregion_vbt *opregionvbt = vdev->region[i].data;
	loff_t pos = *ppos & VFIO_PCI_OFFSET_MASK, off = 0;
	size_t remaining;

	if (pos >= vdev->region[i].size || iswrite)
		return -EINVAL;

	count = min_t(size_t, count, vdev->region[i].size - pos);
	remaining = count;

	 
	if (remaining && pos < OPREGION_VERSION) {
		size_t bytes = min_t(size_t, remaining, OPREGION_VERSION - pos);

		if (igd_opregion_shift_copy(buf, &off,
					    opregionvbt->opregion + pos, &pos,
					    &remaining, bytes))
			return -EFAULT;
	}

	 
	if (remaining && pos < OPREGION_VERSION + sizeof(__le16)) {
		size_t bytes = min_t(size_t, remaining,
				     OPREGION_VERSION + sizeof(__le16) - pos);
		__le16 version = *(__le16 *)(opregionvbt->opregion +
					     OPREGION_VERSION);

		 
		if (le16_to_cpu(version) == 0x0200 && opregionvbt->vbt_ex)
			version = cpu_to_le16(0x0201);

		if (igd_opregion_shift_copy(buf, &off,
					    (u8 *)&version +
					    (pos - OPREGION_VERSION),
					    &pos, &remaining, bytes))
			return -EFAULT;
	}

	 
	if (remaining && pos < OPREGION_RVDA) {
		size_t bytes = min_t(size_t, remaining, OPREGION_RVDA - pos);

		if (igd_opregion_shift_copy(buf, &off,
					    opregionvbt->opregion + pos, &pos,
					    &remaining, bytes))
			return -EFAULT;
	}

	 
	if (remaining && pos < OPREGION_RVDA + sizeof(__le64)) {
		size_t bytes = min_t(size_t, remaining,
				     OPREGION_RVDA + sizeof(__le64) - pos);
		__le64 rvda = cpu_to_le64(opregionvbt->vbt_ex ?
					  OPREGION_SIZE : 0);

		if (igd_opregion_shift_copy(buf, &off,
					    (u8 *)&rvda + (pos - OPREGION_RVDA),
					    &pos, &remaining, bytes))
			return -EFAULT;
	}

	 
	if (remaining && pos < OPREGION_SIZE) {
		size_t bytes = min_t(size_t, remaining, OPREGION_SIZE - pos);

		if (igd_opregion_shift_copy(buf, &off,
					    opregionvbt->opregion + pos, &pos,
					    &remaining, bytes))
			return -EFAULT;
	}

	 
	if (remaining &&
	    copy_to_user(buf + off, opregionvbt->vbt_ex + (pos - OPREGION_SIZE),
			 remaining))
		return -EFAULT;

	*ppos += count;

	return count;
}

static void vfio_pci_igd_release(struct vfio_pci_core_device *vdev,
				 struct vfio_pci_region *region)
{
	struct igd_opregion_vbt *opregionvbt = region->data;

	if (opregionvbt->vbt_ex)
		memunmap(opregionvbt->vbt_ex);

	memunmap(opregionvbt->opregion);
	kfree(opregionvbt);
}

static const struct vfio_pci_regops vfio_pci_igd_regops = {
	.rw		= vfio_pci_igd_rw,
	.release	= vfio_pci_igd_release,
};

static int vfio_pci_igd_opregion_init(struct vfio_pci_core_device *vdev)
{
	__le32 *dwordp = (__le32 *)(vdev->vconfig + OPREGION_PCI_ADDR);
	u32 addr, size;
	struct igd_opregion_vbt *opregionvbt;
	int ret;
	u16 version;

	ret = pci_read_config_dword(vdev->pdev, OPREGION_PCI_ADDR, &addr);
	if (ret)
		return ret;

	if (!addr || !(~addr))
		return -ENODEV;

	opregionvbt = kzalloc(sizeof(*opregionvbt), GFP_KERNEL_ACCOUNT);
	if (!opregionvbt)
		return -ENOMEM;

	opregionvbt->opregion = memremap(addr, OPREGION_SIZE, MEMREMAP_WB);
	if (!opregionvbt->opregion) {
		kfree(opregionvbt);
		return -ENOMEM;
	}

	if (memcmp(opregionvbt->opregion, OPREGION_SIGNATURE, 16)) {
		memunmap(opregionvbt->opregion);
		kfree(opregionvbt);
		return -EINVAL;
	}

	size = le32_to_cpu(*(__le32 *)(opregionvbt->opregion + 16));
	if (!size) {
		memunmap(opregionvbt->opregion);
		kfree(opregionvbt);
		return -EINVAL;
	}

	size *= 1024;  

	 
	version = le16_to_cpu(*(__le16 *)(opregionvbt->opregion +
					  OPREGION_VERSION));
	if (version >= 0x0200) {
		u64 rvda = le64_to_cpu(*(__le64 *)(opregionvbt->opregion +
						   OPREGION_RVDA));
		u32 rvds = le32_to_cpu(*(__le32 *)(opregionvbt->opregion +
						   OPREGION_RVDS));

		 
		if (rvda && rvds) {
			size += rvds;

			 
			if (version == 0x0200)
				addr = rvda;
			else
				addr += rvda;

			opregionvbt->vbt_ex = memremap(addr, rvds, MEMREMAP_WB);
			if (!opregionvbt->vbt_ex) {
				memunmap(opregionvbt->opregion);
				kfree(opregionvbt);
				return -ENOMEM;
			}
		}
	}

	ret = vfio_pci_core_register_dev_region(vdev,
		PCI_VENDOR_ID_INTEL | VFIO_REGION_TYPE_PCI_VENDOR_TYPE,
		VFIO_REGION_SUBTYPE_INTEL_IGD_OPREGION, &vfio_pci_igd_regops,
		size, VFIO_REGION_INFO_FLAG_READ, opregionvbt);
	if (ret) {
		if (opregionvbt->vbt_ex)
			memunmap(opregionvbt->vbt_ex);

		memunmap(opregionvbt->opregion);
		kfree(opregionvbt);
		return ret;
	}

	 
	*dwordp = cpu_to_le32(addr);
	memset(vdev->pci_config_map + OPREGION_PCI_ADDR,
	       PCI_CAP_ID_INVALID_VIRT, 4);

	return ret;
}

static ssize_t vfio_pci_igd_cfg_rw(struct vfio_pci_core_device *vdev,
				   char __user *buf, size_t count, loff_t *ppos,
				   bool iswrite)
{
	unsigned int i = VFIO_PCI_OFFSET_TO_INDEX(*ppos) - VFIO_PCI_NUM_REGIONS;
	struct pci_dev *pdev = vdev->region[i].data;
	loff_t pos = *ppos & VFIO_PCI_OFFSET_MASK;
	size_t size;
	int ret;

	if (pos >= vdev->region[i].size || iswrite)
		return -EINVAL;

	size = count = min(count, (size_t)(vdev->region[i].size - pos));

	if ((pos & 1) && size) {
		u8 val;

		ret = pci_user_read_config_byte(pdev, pos, &val);
		if (ret)
			return ret;

		if (copy_to_user(buf + count - size, &val, 1))
			return -EFAULT;

		pos++;
		size--;
	}

	if ((pos & 3) && size > 2) {
		u16 val;
		__le16 lval;

		ret = pci_user_read_config_word(pdev, pos, &val);
		if (ret)
			return ret;

		lval = cpu_to_le16(val);
		if (copy_to_user(buf + count - size, &lval, 2))
			return -EFAULT;

		pos += 2;
		size -= 2;
	}

	while (size > 3) {
		u32 val;
		__le32 lval;

		ret = pci_user_read_config_dword(pdev, pos, &val);
		if (ret)
			return ret;

		lval = cpu_to_le32(val);
		if (copy_to_user(buf + count - size, &lval, 4))
			return -EFAULT;

		pos += 4;
		size -= 4;
	}

	while (size >= 2) {
		u16 val;
		__le16 lval;

		ret = pci_user_read_config_word(pdev, pos, &val);
		if (ret)
			return ret;

		lval = cpu_to_le16(val);
		if (copy_to_user(buf + count - size, &lval, 2))
			return -EFAULT;

		pos += 2;
		size -= 2;
	}

	while (size) {
		u8 val;

		ret = pci_user_read_config_byte(pdev, pos, &val);
		if (ret)
			return ret;

		if (copy_to_user(buf + count - size, &val, 1))
			return -EFAULT;

		pos++;
		size--;
	}

	*ppos += count;

	return count;
}

static void vfio_pci_igd_cfg_release(struct vfio_pci_core_device *vdev,
				     struct vfio_pci_region *region)
{
	struct pci_dev *pdev = region->data;

	pci_dev_put(pdev);
}

static const struct vfio_pci_regops vfio_pci_igd_cfg_regops = {
	.rw		= vfio_pci_igd_cfg_rw,
	.release	= vfio_pci_igd_cfg_release,
};

static int vfio_pci_igd_cfg_init(struct vfio_pci_core_device *vdev)
{
	struct pci_dev *host_bridge, *lpc_bridge;
	int ret;

	host_bridge = pci_get_domain_bus_and_slot(0, 0, PCI_DEVFN(0, 0));
	if (!host_bridge)
		return -ENODEV;

	if (host_bridge->vendor != PCI_VENDOR_ID_INTEL ||
	    host_bridge->class != (PCI_CLASS_BRIDGE_HOST << 8)) {
		pci_dev_put(host_bridge);
		return -EINVAL;
	}

	ret = vfio_pci_core_register_dev_region(vdev,
		PCI_VENDOR_ID_INTEL | VFIO_REGION_TYPE_PCI_VENDOR_TYPE,
		VFIO_REGION_SUBTYPE_INTEL_IGD_HOST_CFG,
		&vfio_pci_igd_cfg_regops, host_bridge->cfg_size,
		VFIO_REGION_INFO_FLAG_READ, host_bridge);
	if (ret) {
		pci_dev_put(host_bridge);
		return ret;
	}

	lpc_bridge = pci_get_domain_bus_and_slot(0, 0, PCI_DEVFN(0x1f, 0));
	if (!lpc_bridge)
		return -ENODEV;

	if (lpc_bridge->vendor != PCI_VENDOR_ID_INTEL ||
	    lpc_bridge->class != (PCI_CLASS_BRIDGE_ISA << 8)) {
		pci_dev_put(lpc_bridge);
		return -EINVAL;
	}

	ret = vfio_pci_core_register_dev_region(vdev,
		PCI_VENDOR_ID_INTEL | VFIO_REGION_TYPE_PCI_VENDOR_TYPE,
		VFIO_REGION_SUBTYPE_INTEL_IGD_LPC_CFG,
		&vfio_pci_igd_cfg_regops, lpc_bridge->cfg_size,
		VFIO_REGION_INFO_FLAG_READ, lpc_bridge);
	if (ret) {
		pci_dev_put(lpc_bridge);
		return ret;
	}

	return 0;
}

int vfio_pci_igd_init(struct vfio_pci_core_device *vdev)
{
	int ret;

	ret = vfio_pci_igd_opregion_init(vdev);
	if (ret)
		return ret;

	ret = vfio_pci_igd_cfg_init(vdev);
	if (ret)
		return ret;

	return 0;
}
