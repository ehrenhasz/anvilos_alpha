#ifndef _ASM_LOONGARCH_XOR_SIMD_H
#define _ASM_LOONGARCH_XOR_SIMD_H
#ifdef CONFIG_CPU_HAS_LSX
void xor_lsx_2(unsigned long bytes, unsigned long * __restrict p1,
	       const unsigned long * __restrict p2);
void xor_lsx_3(unsigned long bytes, unsigned long * __restrict p1,
	       const unsigned long * __restrict p2, const unsigned long * __restrict p3);
void xor_lsx_4(unsigned long bytes, unsigned long * __restrict p1,
	       const unsigned long * __restrict p2, const unsigned long * __restrict p3,
	       const unsigned long * __restrict p4);
void xor_lsx_5(unsigned long bytes, unsigned long * __restrict p1,
	       const unsigned long * __restrict p2, const unsigned long * __restrict p3,
	       const unsigned long * __restrict p4, const unsigned long * __restrict p5);
#endif  
#ifdef CONFIG_CPU_HAS_LASX
void xor_lasx_2(unsigned long bytes, unsigned long * __restrict p1,
	        const unsigned long * __restrict p2);
void xor_lasx_3(unsigned long bytes, unsigned long * __restrict p1,
	        const unsigned long * __restrict p2, const unsigned long * __restrict p3);
void xor_lasx_4(unsigned long bytes, unsigned long * __restrict p1,
	        const unsigned long * __restrict p2, const unsigned long * __restrict p3,
	        const unsigned long * __restrict p4);
void xor_lasx_5(unsigned long bytes, unsigned long * __restrict p1,
	        const unsigned long * __restrict p2, const unsigned long * __restrict p3,
	        const unsigned long * __restrict p4, const unsigned long * __restrict p5);
#endif  
#endif  
