 
#ifndef _CGS_COMMON_H
#define _CGS_COMMON_H

#include "amd_shared.h"

struct cgs_device;

 
enum cgs_ind_reg {
	CGS_IND_REG__PCIE,
	CGS_IND_REG__SMC,
	CGS_IND_REG__UVD_CTX,
	CGS_IND_REG__DIDT,
	CGS_IND_REG_GC_CAC,
	CGS_IND_REG_SE_CAC,
	CGS_IND_REG__AUDIO_ENDPT
};

 
enum cgs_ucode_id {
	CGS_UCODE_ID_SMU = 0,
	CGS_UCODE_ID_SMU_SK,
	CGS_UCODE_ID_SDMA0,
	CGS_UCODE_ID_SDMA1,
	CGS_UCODE_ID_CP_CE,
	CGS_UCODE_ID_CP_PFP,
	CGS_UCODE_ID_CP_ME,
	CGS_UCODE_ID_CP_MEC,
	CGS_UCODE_ID_CP_MEC_JT1,
	CGS_UCODE_ID_CP_MEC_JT2,
	CGS_UCODE_ID_GMCON_RENG,
	CGS_UCODE_ID_RLC_G,
	CGS_UCODE_ID_STORAGE,
	CGS_UCODE_ID_MAXIMUM,
};

 
struct cgs_firmware_info {
	uint16_t		version;
	uint16_t		fw_version;
	uint16_t		feature_version;
	uint32_t		image_size;
	uint64_t		mc_addr;

	 
	uint32_t		ucode_start_address;

	void			*kptr;
	bool			is_kicker;
};

typedef unsigned long cgs_handle_t;

 
typedef uint32_t (*cgs_read_register_t)(struct cgs_device *cgs_device, unsigned offset);

 
typedef void (*cgs_write_register_t)(struct cgs_device *cgs_device, unsigned offset,
				     uint32_t value);

 
typedef uint32_t (*cgs_read_ind_register_t)(struct cgs_device *cgs_device, enum cgs_ind_reg space,
					    unsigned index);

 
typedef void (*cgs_write_ind_register_t)(struct cgs_device *cgs_device, enum cgs_ind_reg space,
					 unsigned index, uint32_t value);

#define CGS_REG_FIELD_SHIFT(reg, field) reg##__##field##__SHIFT
#define CGS_REG_FIELD_MASK(reg, field) reg##__##field##_MASK

#define CGS_REG_SET_FIELD(orig_val, reg, field, field_val)			\
	(((orig_val) & ~CGS_REG_FIELD_MASK(reg, field)) |			\
	 (CGS_REG_FIELD_MASK(reg, field) & ((field_val) << CGS_REG_FIELD_SHIFT(reg, field))))

#define CGS_REG_GET_FIELD(value, reg, field)				\
	(((value) & CGS_REG_FIELD_MASK(reg, field)) >> CGS_REG_FIELD_SHIFT(reg, field))

#define CGS_WREG32_FIELD(device, reg, field, val)	\
	cgs_write_register(device, mm##reg, (cgs_read_register(device, mm##reg) & ~CGS_REG_FIELD_MASK(reg, field)) | (val) << CGS_REG_FIELD_SHIFT(reg, field))

#define CGS_WREG32_FIELD_IND(device, space, reg, field, val)	\
	cgs_write_ind_register(device, space, ix##reg, (cgs_read_ind_register(device, space, ix##reg) & ~CGS_REG_FIELD_MASK(reg, field)) | (val) << CGS_REG_FIELD_SHIFT(reg, field))

typedef int (*cgs_get_firmware_info)(struct cgs_device *cgs_device,
				     enum cgs_ucode_id type,
				     struct cgs_firmware_info *info);

struct cgs_ops {
	 
	cgs_read_register_t read_register;
	cgs_write_register_t write_register;
	cgs_read_ind_register_t read_ind_register;
	cgs_write_ind_register_t write_ind_register;
	 
	cgs_get_firmware_info get_firmware_info;
};

struct cgs_os_ops;  

struct cgs_device
{
	const struct cgs_ops *ops;
	 
};

 
#define CGS_CALL(func,dev,...) \
	(((struct cgs_device *)dev)->ops->func(dev, ##__VA_ARGS__))
#define CGS_OS_CALL(func,dev,...) \
	(((struct cgs_device *)dev)->os_ops->func(dev, ##__VA_ARGS__))

#define cgs_read_register(dev,offset)		\
	CGS_CALL(read_register,dev,offset)
#define cgs_write_register(dev,offset,value)		\
	CGS_CALL(write_register,dev,offset,value)
#define cgs_read_ind_register(dev,space,index)		\
	CGS_CALL(read_ind_register,dev,space,index)
#define cgs_write_ind_register(dev,space,index,value)		\
	CGS_CALL(write_ind_register,dev,space,index,value)

#define cgs_get_firmware_info(dev, type, info)	\
	CGS_CALL(get_firmware_info, dev, type, info)

#endif  
