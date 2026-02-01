 
#include <linux/pci.h>

#include "amdgpu.h"
#include "amdgpu_i2c.h"
#include "smu_v11_0_i2c.h"
#include "atom.h"
#include "amdgpu_fru_eeprom.h"
#include "amdgpu_eeprom.h"

#define FRU_EEPROM_MADDR_6      0x60000
#define FRU_EEPROM_MADDR_8      0x80000

static bool is_fru_eeprom_supported(struct amdgpu_device *adev, u32 *fru_addr)
{
	 
	struct atom_context *atom_ctx = adev->mode_info.atom_context;

	 
	if (amdgpu_sriov_vf(adev))
		return false;

	 
	if (fru_addr)
		*fru_addr = FRU_EEPROM_MADDR_8;

	 
	switch (adev->asic_type) {
	case CHIP_VEGA20:
		 
		if (strnstr(atom_ctx->vbios_pn, "D161",
			    sizeof(atom_ctx->vbios_pn)) ||
		    strnstr(atom_ctx->vbios_pn, "D163",
			    sizeof(atom_ctx->vbios_pn))) {
			if (fru_addr)
				*fru_addr = FRU_EEPROM_MADDR_6;
			return true;
		} else {
			return false;
		}
	case CHIP_ALDEBARAN:
		 
		if (!strnstr(atom_ctx->vbios_pn, "D673",
			     sizeof(atom_ctx->vbios_pn)))
			if (fru_addr)
				*fru_addr = FRU_EEPROM_MADDR_6;
		return true;
	case CHIP_SIENNA_CICHLID:
		if (strnstr(atom_ctx->vbios_pn, "D603",
			    sizeof(atom_ctx->vbios_pn))) {
			if (strnstr(atom_ctx->vbios_pn, "D603GLXE",
				    sizeof(atom_ctx->vbios_pn))) {
				return false;
			}

			if (fru_addr)
				*fru_addr = FRU_EEPROM_MADDR_6;
			return true;

		} else {
			return false;
		}
	default:
		return false;
	}
}

int amdgpu_fru_get_product_info(struct amdgpu_device *adev)
{
	unsigned char buf[8], *pia;
	u32 addr, fru_addr;
	int size, len;
	u8 csum;

	if (!is_fru_eeprom_supported(adev, &fru_addr))
		return 0;

	 
	if (!adev->pm.fru_eeprom_i2c_bus || !adev->pm.fru_eeprom_i2c_bus->algo) {
		DRM_WARN("Cannot access FRU, EEPROM accessor not initialized");
		return -ENODEV;
	}

	 
	len = amdgpu_eeprom_read(adev->pm.fru_eeprom_i2c_bus, fru_addr, buf,
				 sizeof(buf));
	if (len != 8) {
		DRM_ERROR("Couldn't read the IPMI Common Header: %d", len);
		return len < 0 ? len : -EIO;
	}

	if (buf[0] != 1) {
		DRM_ERROR("Bad IPMI Common Header version: 0x%02x", buf[0]);
		return -EIO;
	}

	for (csum = 0; len > 0; len--)
		csum += buf[len - 1];
	if (csum) {
		DRM_ERROR("Bad IPMI Common Header checksum: 0x%02x", csum);
		return -EIO;
	}

	 
	addr = buf[4] * 8;
	if (!addr)
		return 0;

	 
	addr += fru_addr;

	 
	len = amdgpu_eeprom_read(adev->pm.fru_eeprom_i2c_bus, addr, buf, 3);
	if (len != 3) {
		DRM_ERROR("Couldn't read the Product Info Area header: %d", len);
		return len < 0 ? len : -EIO;
	}

	if (buf[0] != 1) {
		DRM_ERROR("Bad IPMI Product Info Area version: 0x%02x", buf[0]);
		return -EIO;
	}

	size = buf[1] * 8;
	pia = kzalloc(size, GFP_KERNEL);
	if (!pia)
		return -ENOMEM;

	 
	len = amdgpu_eeprom_read(adev->pm.fru_eeprom_i2c_bus, addr, pia, size);
	if (len != size) {
		kfree(pia);
		DRM_ERROR("Couldn't read the Product Info Area: %d", len);
		return len < 0 ? len : -EIO;
	}

	for (csum = 0; size > 0; size--)
		csum += pia[size - 1];
	if (csum) {
		DRM_ERROR("Bad Product Info Area checksum: 0x%02x", csum);
		kfree(pia);
		return -EIO;
	}

	 
	addr = 3 + 1 + (pia[3] & 0x3F);
	if (addr + 1 >= len)
		goto Out;
	memcpy(adev->product_name, pia + addr + 1,
	       min_t(size_t,
		     sizeof(adev->product_name),
		     pia[addr] & 0x3F));
	adev->product_name[sizeof(adev->product_name) - 1] = '\0';

	 
	addr += 1 + (pia[addr] & 0x3F);
	if (addr + 1 >= len)
		goto Out;
	memcpy(adev->product_number, pia + addr + 1,
	       min_t(size_t,
		     sizeof(adev->product_number),
		     pia[addr] & 0x3F));
	adev->product_number[sizeof(adev->product_number) - 1] = '\0';

	 
	addr += 1 + (pia[addr] & 0x3F);

	 
	addr += 1 + (pia[addr] & 0x3F);
	if (addr + 1 >= len)
		goto Out;
	memcpy(adev->serial, pia + addr + 1, min_t(size_t,
						   sizeof(adev->serial),
						   pia[addr] & 0x3F));
	adev->serial[sizeof(adev->serial) - 1] = '\0';
Out:
	kfree(pia);
	return 0;
}

 

static ssize_t amdgpu_fru_product_name_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);

	return sysfs_emit(buf, "%s\n", adev->product_name);
}

static DEVICE_ATTR(product_name, 0444, amdgpu_fru_product_name_show, NULL);

 

static ssize_t amdgpu_fru_product_number_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);

	return sysfs_emit(buf, "%s\n", adev->product_number);
}

static DEVICE_ATTR(product_number, 0444, amdgpu_fru_product_number_show, NULL);

 

static ssize_t amdgpu_fru_serial_number_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);

	return sysfs_emit(buf, "%s\n", adev->serial);
}

static DEVICE_ATTR(serial_number, 0444, amdgpu_fru_serial_number_show, NULL);

static const struct attribute *amdgpu_fru_attributes[] = {
	&dev_attr_product_name.attr,
	&dev_attr_product_number.attr,
	&dev_attr_serial_number.attr,
	NULL
};

int amdgpu_fru_sysfs_init(struct amdgpu_device *adev)
{
	if (!is_fru_eeprom_supported(adev, NULL))
		return 0;

	return sysfs_create_files(&adev->dev->kobj, amdgpu_fru_attributes);
}

void amdgpu_fru_sysfs_fini(struct amdgpu_device *adev)
{
	if (!is_fru_eeprom_supported(adev, NULL))
		return;

	sysfs_remove_files(&adev->dev->kobj, amdgpu_fru_attributes);
}
