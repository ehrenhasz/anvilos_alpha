#ifndef __MIPS_ASM_MIPS_CPS_H__
# error Please include asm/mips-cps.h rather than asm/mips-cm.h
#endif
#ifndef __MIPS_ASM_MIPS_CM_H__
#define __MIPS_ASM_MIPS_CM_H__
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/errno.h>
extern void __iomem *mips_gcr_base;
extern void __iomem *mips_cm_l2sync_base;
extern phys_addr_t __mips_cm_phys_base(void);
extern int mips_cm_is64;
#ifdef CONFIG_MIPS_CM
extern void mips_cm_error_report(void);
#else
static inline void mips_cm_error_report(void) {}
#endif
#ifdef CONFIG_MIPS_CM
extern int mips_cm_probe(void);
#else
static inline int mips_cm_probe(void)
{
	return -ENODEV;
}
#endif
static inline bool mips_cm_present(void)
{
#ifdef CONFIG_MIPS_CM
	return mips_gcr_base != NULL;
#else
	return false;
#endif
}
static inline bool mips_cm_has_l2sync(void)
{
#ifdef CONFIG_MIPS_CM
	return mips_cm_l2sync_base != NULL;
#else
	return false;
#endif
}
#define MIPS_CM_GCB_OFS		0x0000  
#define MIPS_CM_CLCB_OFS	0x2000  
#define MIPS_CM_COCB_OFS	0x4000  
#define MIPS_CM_GDB_OFS		0x6000  
#define MIPS_CM_GCR_SIZE	0x8000
#define MIPS_CM_L2SYNC_SIZE	0x1000
#define GCR_ACCESSOR_RO(sz, off, name)					\
	CPS_ACCESSOR_RO(gcr, sz, MIPS_CM_GCB_OFS + off, name)		\
	CPS_ACCESSOR_RO(gcr, sz, MIPS_CM_COCB_OFS + off, redir_##name)
#define GCR_ACCESSOR_RW(sz, off, name)					\
	CPS_ACCESSOR_RW(gcr, sz, MIPS_CM_GCB_OFS + off, name)		\
	CPS_ACCESSOR_RW(gcr, sz, MIPS_CM_COCB_OFS + off, redir_##name)
#define GCR_CX_ACCESSOR_RO(sz, off, name)				\
	CPS_ACCESSOR_RO(gcr, sz, MIPS_CM_CLCB_OFS + off, cl_##name)	\
	CPS_ACCESSOR_RO(gcr, sz, MIPS_CM_COCB_OFS + off, co_##name)
#define GCR_CX_ACCESSOR_RW(sz, off, name)				\
	CPS_ACCESSOR_RW(gcr, sz, MIPS_CM_CLCB_OFS + off, cl_##name)	\
	CPS_ACCESSOR_RW(gcr, sz, MIPS_CM_COCB_OFS + off, co_##name)
GCR_ACCESSOR_RO(64, 0x000, config)
#define CM_GCR_CONFIG_CLUSTER_COH_CAPABLE	BIT_ULL(43)
#define CM_GCR_CONFIG_CLUSTER_ID		GENMASK_ULL(39, 32)
#define CM_GCR_CONFIG_NUM_CLUSTERS		GENMASK(29, 23)
#define CM_GCR_CONFIG_NUMIOCU			GENMASK(15, 8)
#define CM_GCR_CONFIG_PCORES			GENMASK(7, 0)
GCR_ACCESSOR_RW(64, 0x008, base)
#define CM_GCR_BASE_GCRBASE			GENMASK_ULL(47, 15)
#define CM_GCR_BASE_CMDEFTGT			GENMASK(1, 0)
#define  CM_GCR_BASE_CMDEFTGT_MEM		0
#define  CM_GCR_BASE_CMDEFTGT_RESERVED		1
#define  CM_GCR_BASE_CMDEFTGT_IOCU0		2
#define  CM_GCR_BASE_CMDEFTGT_IOCU1		3
GCR_ACCESSOR_RW(32, 0x020, access)
#define CM_GCR_ACCESS_ACCESSEN			GENMASK(7, 0)
GCR_ACCESSOR_RO(32, 0x030, rev)
#define CM_GCR_REV_MAJOR			GENMASK(15, 8)
#define CM_GCR_REV_MINOR			GENMASK(7, 0)
#define CM_ENCODE_REV(major, minor) \
		(FIELD_PREP(CM_GCR_REV_MAJOR, major) | \
		 FIELD_PREP(CM_GCR_REV_MINOR, minor))
#define CM_REV_CM2				CM_ENCODE_REV(6, 0)
#define CM_REV_CM2_5				CM_ENCODE_REV(7, 0)
#define CM_REV_CM3				CM_ENCODE_REV(8, 0)
#define CM_REV_CM3_5				CM_ENCODE_REV(9, 0)
GCR_ACCESSOR_RW(32, 0x038, err_control)
#define CM_GCR_ERR_CONTROL_L2_ECC_EN		BIT(1)
#define CM_GCR_ERR_CONTROL_L2_ECC_SUPPORT	BIT(0)
GCR_ACCESSOR_RW(64, 0x040, error_mask)
GCR_ACCESSOR_RW(64, 0x048, error_cause)
#define CM_GCR_ERROR_CAUSE_ERRTYPE		GENMASK(31, 27)
#define CM3_GCR_ERROR_CAUSE_ERRTYPE		GENMASK_ULL(63, 58)
#define CM_GCR_ERROR_CAUSE_ERRINFO		GENMASK(26, 0)
GCR_ACCESSOR_RW(64, 0x050, error_addr)
GCR_ACCESSOR_RW(64, 0x058, error_mult)
#define CM_GCR_ERROR_MULT_ERR2ND		GENMASK(4, 0)
GCR_ACCESSOR_RW(64, 0x070, l2_only_sync_base)
#define CM_GCR_L2_ONLY_SYNC_BASE_SYNCBASE	GENMASK(31, 12)
#define CM_GCR_L2_ONLY_SYNC_BASE_SYNCEN		BIT(0)
GCR_ACCESSOR_RW(64, 0x080, gic_base)
#define CM_GCR_GIC_BASE_GICBASE			GENMASK(31, 17)
#define CM_GCR_GIC_BASE_GICEN			BIT(0)
GCR_ACCESSOR_RW(64, 0x088, cpc_base)
#define CM_GCR_CPC_BASE_CPCBASE			GENMASK(31, 15)
#define CM_GCR_CPC_BASE_CPCEN			BIT(0)
GCR_ACCESSOR_RW(64, 0x090, reg0_base)
GCR_ACCESSOR_RW(64, 0x0a0, reg1_base)
GCR_ACCESSOR_RW(64, 0x0b0, reg2_base)
GCR_ACCESSOR_RW(64, 0x0c0, reg3_base)
#define CM_GCR_REGn_BASE_BASEADDR		GENMASK(31, 16)
GCR_ACCESSOR_RW(64, 0x098, reg0_mask)
GCR_ACCESSOR_RW(64, 0x0a8, reg1_mask)
GCR_ACCESSOR_RW(64, 0x0b8, reg2_mask)
GCR_ACCESSOR_RW(64, 0x0c8, reg3_mask)
#define CM_GCR_REGn_MASK_ADDRMASK		GENMASK(31, 16)
#define CM_GCR_REGn_MASK_CCAOVR			GENMASK(7, 5)
#define CM_GCR_REGn_MASK_CCAOVREN		BIT(4)
#define CM_GCR_REGn_MASK_DROPL2			BIT(2)
#define CM_GCR_REGn_MASK_CMTGT			GENMASK(1, 0)
#define  CM_GCR_REGn_MASK_CMTGT_DISABLED	0x0
#define  CM_GCR_REGn_MASK_CMTGT_MEM		0x1
#define  CM_GCR_REGn_MASK_CMTGT_IOCU0		0x2
#define  CM_GCR_REGn_MASK_CMTGT_IOCU1		0x3
GCR_ACCESSOR_RO(32, 0x0d0, gic_status)
#define CM_GCR_GIC_STATUS_EX			BIT(0)
GCR_ACCESSOR_RO(32, 0x0f0, cpc_status)
#define CM_GCR_CPC_STATUS_EX			BIT(0)
GCR_ACCESSOR_RW(32, 0x130, l2_config)
#define CM_GCR_L2_CONFIG_BYPASS			BIT(20)
#define CM_GCR_L2_CONFIG_SET_SIZE		GENMASK(15, 12)
#define CM_GCR_L2_CONFIG_LINE_SIZE		GENMASK(11, 8)
#define CM_GCR_L2_CONFIG_ASSOC			GENMASK(7, 0)
GCR_ACCESSOR_RO(32, 0x150, sys_config2)
#define CM_GCR_SYS_CONFIG2_MAXVPW		GENMASK(3, 0)
GCR_ACCESSOR_RW(32, 0x300, l2_pft_control)
#define CM_GCR_L2_PFT_CONTROL_PAGEMASK		GENMASK(31, 12)
#define CM_GCR_L2_PFT_CONTROL_PFTEN		BIT(8)
#define CM_GCR_L2_PFT_CONTROL_NPFT		GENMASK(7, 0)
GCR_ACCESSOR_RW(32, 0x308, l2_pft_control_b)
#define CM_GCR_L2_PFT_CONTROL_B_CEN		BIT(8)
#define CM_GCR_L2_PFT_CONTROL_B_PORTID		GENMASK(7, 0)
GCR_ACCESSOR_RW(32, 0x620, l2sm_cop)
#define CM_GCR_L2SM_COP_PRESENT			BIT(31)
#define CM_GCR_L2SM_COP_RESULT			GENMASK(8, 6)
#define  CM_GCR_L2SM_COP_RESULT_DONTCARE	0
#define  CM_GCR_L2SM_COP_RESULT_DONE_OK		1
#define  CM_GCR_L2SM_COP_RESULT_DONE_ERROR	2
#define  CM_GCR_L2SM_COP_RESULT_ABORT_OK	3
#define  CM_GCR_L2SM_COP_RESULT_ABORT_ERROR	4
#define CM_GCR_L2SM_COP_RUNNING			BIT(5)
#define CM_GCR_L2SM_COP_TYPE			GENMASK(4, 2)
#define  CM_GCR_L2SM_COP_TYPE_IDX_WBINV		0
#define  CM_GCR_L2SM_COP_TYPE_IDX_STORETAG	1
#define  CM_GCR_L2SM_COP_TYPE_IDX_STORETAGDATA	2
#define  CM_GCR_L2SM_COP_TYPE_HIT_INV		4
#define  CM_GCR_L2SM_COP_TYPE_HIT_WBINV		5
#define  CM_GCR_L2SM_COP_TYPE_HIT_WB		6
#define  CM_GCR_L2SM_COP_TYPE_FETCHLOCK		7
#define CM_GCR_L2SM_COP_CMD			GENMASK(1, 0)
#define  CM_GCR_L2SM_COP_CMD_START		1	 
#define  CM_GCR_L2SM_COP_CMD_ABORT		3	 
GCR_ACCESSOR_RW(64, 0x628, l2sm_tag_addr_cop)
#define CM_GCR_L2SM_TAG_ADDR_COP_NUM_LINES	GENMASK_ULL(63, 48)
#define CM_GCR_L2SM_TAG_ADDR_COP_START_TAG	GENMASK_ULL(47, 6)
GCR_ACCESSOR_RW(64, 0x680, bev_base)
GCR_CX_ACCESSOR_RW(32, 0x000, reset_release)
GCR_CX_ACCESSOR_RW(32, 0x008, coherence)
#define CM_GCR_Cx_COHERENCE_COHDOMAINEN		GENMASK(7, 0)
#define CM3_GCR_Cx_COHERENCE_COHEN		BIT(0)
GCR_CX_ACCESSOR_RO(32, 0x010, config)
#define CM_GCR_Cx_CONFIG_IOCUTYPE		GENMASK(11, 10)
#define CM_GCR_Cx_CONFIG_PVPE			GENMASK(9, 0)
GCR_CX_ACCESSOR_RW(32, 0x018, other)
#define CM_GCR_Cx_OTHER_CORENUM			GENMASK(31, 16)	 
#define CM_GCR_Cx_OTHER_CLUSTER_EN		BIT(31)		 
#define CM_GCR_Cx_OTHER_GIC_EN			BIT(30)		 
#define CM_GCR_Cx_OTHER_BLOCK			GENMASK(25, 24)	 
#define  CM_GCR_Cx_OTHER_BLOCK_LOCAL		0
#define  CM_GCR_Cx_OTHER_BLOCK_GLOBAL		1
#define  CM_GCR_Cx_OTHER_BLOCK_USER		2
#define  CM_GCR_Cx_OTHER_BLOCK_GLOBAL_HIGH	3
#define CM_GCR_Cx_OTHER_CLUSTER			GENMASK(21, 16)	 
#define CM3_GCR_Cx_OTHER_CORE			GENMASK(13, 8)	 
#define  CM_GCR_Cx_OTHER_CORE_CM		32
#define CM3_GCR_Cx_OTHER_VP			GENMASK(2, 0)	 
GCR_CX_ACCESSOR_RW(32, 0x020, reset_base)
#define CM_GCR_Cx_RESET_BASE_BEVEXCBASE		GENMASK(31, 12)
GCR_CX_ACCESSOR_RO(32, 0x028, id)
#define CM_GCR_Cx_ID_CLUSTER			GENMASK(15, 8)
#define CM_GCR_Cx_ID_CORE			GENMASK(7, 0)
GCR_CX_ACCESSOR_RW(32, 0x030, reset_ext_base)
#define CM_GCR_Cx_RESET_EXT_BASE_EVARESET	BIT(31)
#define CM_GCR_Cx_RESET_EXT_BASE_UEB		BIT(30)
#define CM_GCR_Cx_RESET_EXT_BASE_BEVEXCMASK	GENMASK(27, 20)
#define CM_GCR_Cx_RESET_EXT_BASE_BEVEXCPA	GENMASK(7, 1)
#define CM_GCR_Cx_RESET_EXT_BASE_PRESENT	BIT(0)
static inline int mips_cm_l2sync(void)
{
	if (!mips_cm_has_l2sync())
		return -ENODEV;
	writel(0, mips_cm_l2sync_base);
	return 0;
}
static inline int mips_cm_revision(void)
{
	if (!mips_cm_present())
		return 0;
	return read_gcr_rev();
}
static inline unsigned int mips_cm_max_vp_width(void)
{
	extern int smp_num_siblings;
	if (mips_cm_revision() >= CM_REV_CM3)
		return FIELD_GET(CM_GCR_SYS_CONFIG2_MAXVPW,
				 read_gcr_sys_config2());
	if (mips_cm_present()) {
		return FIELD_GET(CM_GCR_Cx_CONFIG_PVPE, read_gcr_cl_config()) + 1;
	}
	if (IS_ENABLED(CONFIG_SMP))
		return smp_num_siblings;
	return 1;
}
static inline unsigned int mips_cm_vp_id(unsigned int cpu)
{
	unsigned int core = cpu_core(&cpu_data[cpu]);
	unsigned int vp = cpu_vpe_id(&cpu_data[cpu]);
	return (core * mips_cm_max_vp_width()) + vp;
}
#ifdef CONFIG_MIPS_CM
extern void mips_cm_lock_other(unsigned int cluster, unsigned int core,
			       unsigned int vp, unsigned int block);
extern void mips_cm_unlock_other(void);
#else  
static inline void mips_cm_lock_other(unsigned int cluster, unsigned int core,
				      unsigned int vp, unsigned int block) { }
static inline void mips_cm_unlock_other(void) { }
#endif  
static inline void mips_cm_lock_other_cpu(unsigned int cpu, unsigned int block)
{
	struct cpuinfo_mips *d = &cpu_data[cpu];
	mips_cm_lock_other(cpu_cluster(d), cpu_core(d), cpu_vpe_id(d), block);
}
#endif  
