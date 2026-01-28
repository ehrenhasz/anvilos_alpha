asmlinkage void do_sparc_fault(struct pt_regs *regs, int text_fault, int write,
                               unsigned long address);
void window_overflow_fault(void);
void window_underflow_fault(unsigned long sp);
void window_ret_fault(struct pt_regs *regs);
extern char *srmmu_name;
extern int viking_mxcc_present;
extern int flush_page_for_dma_global;
extern void (*poke_srmmu)(void);
void __init srmmu_paging_init(void);
void ld_mmu_iommu(void);
