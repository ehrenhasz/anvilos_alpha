#ifndef __ASM_ARC_DSP_H
#define __ASM_ARC_DSP_H
#ifndef __ASSEMBLY__
struct dsp_callee_regs {
	unsigned long ACC0_GLO, ACC0_GHI, DSP_BFLY0, DSP_FFT_CTRL;
#ifdef CONFIG_ARC_DSP_AGU_USERSPACE
	unsigned long AGU_AP0, AGU_AP1, AGU_AP2, AGU_AP3;
	unsigned long AGU_OS0, AGU_OS1;
	unsigned long AGU_MOD0, AGU_MOD1, AGU_MOD2, AGU_MOD3;
#endif
};
#endif  
#endif  
