#ifndef _ASM_MSHYPERV_H
#define _ASM_MSHYPERV_H
#include <linux/types.h>
#include <linux/arm-smccc.h>
#include <asm/hyperv-tlfs.h>
void hv_set_vpreg(u32 reg, u64 value);
u64 hv_get_vpreg(u32 reg);
void hv_get_vpreg_128(u32 reg, struct hv_get_vp_registers_output *result);
static inline void hv_set_register(unsigned int reg, u64 value)
{
	hv_set_vpreg(reg, value);
}
static inline u64 hv_get_register(unsigned int reg)
{
	return hv_get_vpreg(reg);
}
#define HV_SMCCC_FUNC_NUMBER	1
#define HV_FUNC_ID	ARM_SMCCC_CALL_VAL(			\
				ARM_SMCCC_STD_CALL,		\
				ARM_SMCCC_SMC_64,		\
				ARM_SMCCC_OWNER_VENDOR_HYP,	\
				HV_SMCCC_FUNC_NUMBER)
#include <asm-generic/mshyperv.h>
#endif
