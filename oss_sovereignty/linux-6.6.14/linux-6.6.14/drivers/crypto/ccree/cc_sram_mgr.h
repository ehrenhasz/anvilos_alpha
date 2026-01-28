#ifndef __CC_SRAM_MGR_H__
#define __CC_SRAM_MGR_H__
#ifndef CC_CC_SRAM_SIZE
#define CC_CC_SRAM_SIZE 4096
#endif
struct cc_drvdata;
#define NULL_SRAM_ADDR ((u32)-1)
int cc_sram_mgr_init(struct cc_drvdata *drvdata);
u32 cc_sram_alloc(struct cc_drvdata *drvdata, u32 size);
void cc_set_sram_desc(const u32 *src, u32 dst, unsigned int nelement,
		      struct cc_hw_desc *seq, unsigned int *seq_len);
#endif  
