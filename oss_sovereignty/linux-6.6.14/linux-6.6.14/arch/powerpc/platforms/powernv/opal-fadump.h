#ifndef _POWERNV_OPAL_FADUMP_H
#define _POWERNV_OPAL_FADUMP_H
#include <asm/reg.h>
#define OPAL_FADUMP_MIN_BOOT_MEM		(0x30000000UL)
#define OPAL_FADUMP_VERSION			0x1
struct opal_fadump_mem_struct {
	u8	version;
	u8	reserved[3];
	__be16	region_cnt;		 
	__be16	registered_regions;	 
	__be64	fadumphdr_addr;
	struct opal_mpipl_region	rgn[FADUMP_MAX_MEM_REGS];
} __packed;
#define HDAT_FADUMP_CPU_DATA_VER		1
#define HDAT_FADUMP_CORE_INACTIVE		(0x0F)
struct hdat_fadump_thread_hdr {
	__be32  pir;
	u8      core_state;
	u8      reserved[3];
	__be32	offset;	 
	__be32	ecnt;	 
	__be32	esize;	 
	__be32	eactsz;	 
} __packed;
#define HDAT_FADUMP_REG_TYPE_GPR		0x01
#define HDAT_FADUMP_REG_TYPE_SPR		0x02
#define HDAT_FADUMP_REG_ID_NIP			0x7D0
#define HDAT_FADUMP_REG_ID_MSR			0x7D1
#define HDAT_FADUMP_REG_ID_CCR			0x7D2
struct hdat_fadump_reg_entry {
	__be32		reg_type;
	__be32		reg_num;
	__be64		reg_val;
} __packed;
static inline void opal_fadump_set_regval_regnum(struct pt_regs *regs,
						 u32 reg_type, u32 reg_num,
						 u64 reg_val)
{
	if (reg_type == HDAT_FADUMP_REG_TYPE_GPR) {
		if (reg_num < 32)
			regs->gpr[reg_num] = reg_val;
		return;
	}
	switch (reg_num) {
	case SPRN_CTR:
		regs->ctr = reg_val;
		break;
	case SPRN_LR:
		regs->link = reg_val;
		break;
	case SPRN_XER:
		regs->xer = reg_val;
		break;
	case SPRN_DAR:
		regs->dar = reg_val;
		break;
	case SPRN_DSISR:
		regs->dsisr = reg_val;
		break;
	case HDAT_FADUMP_REG_ID_NIP:
		regs->nip = reg_val;
		break;
	case HDAT_FADUMP_REG_ID_MSR:
		regs->msr = reg_val;
		break;
	case HDAT_FADUMP_REG_ID_CCR:
		regs->ccr = reg_val;
		break;
	}
}
static inline void opal_fadump_read_regs(char *bufp, unsigned int regs_cnt,
					 unsigned int reg_entry_size,
					 bool cpu_endian,
					 struct pt_regs *regs)
{
	struct hdat_fadump_reg_entry *reg_entry;
	u64 val;
	int i;
	memset(regs, 0, sizeof(struct pt_regs));
	for (i = 0; i < regs_cnt; i++, bufp += reg_entry_size) {
		reg_entry = (struct hdat_fadump_reg_entry *)bufp;
		val = (cpu_endian ? be64_to_cpu(reg_entry->reg_val) :
		       (u64)(reg_entry->reg_val));
		opal_fadump_set_regval_regnum(regs,
					      be32_to_cpu(reg_entry->reg_type),
					      be32_to_cpu(reg_entry->reg_num),
					      val);
	}
}
#endif  
