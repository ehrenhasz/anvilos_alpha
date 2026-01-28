


#include <linux/linkage.h>


.section .rodata
.align 16

.Lconstant_R2R1:
	.octa 0x00000001c6e415960000000154442bd4

.Lconstant_R4R3:
	.octa 0x00000000ccaa009e00000001751997d0

.Lconstant_R5:
	.octa 0x00000000000000000000000163cd6124
.Lconstant_mask32:
	.octa 0x000000000000000000000000FFFFFFFF

.Lconstant_RUpoly:
	.octa 0x00000001F701164100000001DB710641

#define CONSTANT %xmm0

#ifdef __x86_64__
#define BUF     %rdi
#define LEN     %rsi
#define CRC     %edx
#else
#define BUF     %eax
#define LEN     %edx
#define CRC     %ecx
#endif



.text


SYM_FUNC_START(crc32_pclmul_le_16) 
	movdqa  (BUF), %xmm1
	movdqa  0x10(BUF), %xmm2
	movdqa  0x20(BUF), %xmm3
	movdqa  0x30(BUF), %xmm4
	movd    CRC, CONSTANT
	pxor    CONSTANT, %xmm1
	sub     $0x40, LEN
	add     $0x40, BUF
	cmp     $0x40, LEN
	jb      .Lless_64

#ifdef __x86_64__
	movdqa .Lconstant_R2R1(%rip), CONSTANT
#else
	movdqa .Lconstant_R2R1, CONSTANT
#endif

.Lloop_64:
	prefetchnta    0x40(BUF)
	movdqa  %xmm1, %xmm5
	movdqa  %xmm2, %xmm6
	movdqa  %xmm3, %xmm7
#ifdef __x86_64__
	movdqa  %xmm4, %xmm8
#endif
	pclmulqdq $0x00, CONSTANT, %xmm1
	pclmulqdq $0x00, CONSTANT, %xmm2
	pclmulqdq $0x00, CONSTANT, %xmm3
#ifdef __x86_64__
	pclmulqdq $0x00, CONSTANT, %xmm4
#endif
	pclmulqdq $0x11, CONSTANT, %xmm5
	pclmulqdq $0x11, CONSTANT, %xmm6
	pclmulqdq $0x11, CONSTANT, %xmm7
#ifdef __x86_64__
	pclmulqdq $0x11, CONSTANT, %xmm8
#endif
	pxor    %xmm5, %xmm1
	pxor    %xmm6, %xmm2
	pxor    %xmm7, %xmm3
#ifdef __x86_64__
	pxor    %xmm8, %xmm4
#else
	
	movdqa  %xmm4, %xmm5
	pclmulqdq $0x00, CONSTANT, %xmm4
	pclmulqdq $0x11, CONSTANT, %xmm5
	pxor    %xmm5, %xmm4
#endif

	pxor    (BUF), %xmm1
	pxor    0x10(BUF), %xmm2
	pxor    0x20(BUF), %xmm3
	pxor    0x30(BUF), %xmm4

	sub     $0x40, LEN
	add     $0x40, BUF
	cmp     $0x40, LEN
	jge     .Lloop_64
.Lless_64:
#ifdef __x86_64__
	movdqa  .Lconstant_R4R3(%rip), CONSTANT
#else
	movdqa  .Lconstant_R4R3, CONSTANT
#endif
	prefetchnta     (BUF)

	movdqa  %xmm1, %xmm5
	pclmulqdq $0x00, CONSTANT, %xmm1
	pclmulqdq $0x11, CONSTANT, %xmm5
	pxor    %xmm5, %xmm1
	pxor    %xmm2, %xmm1

	movdqa  %xmm1, %xmm5
	pclmulqdq $0x00, CONSTANT, %xmm1
	pclmulqdq $0x11, CONSTANT, %xmm5
	pxor    %xmm5, %xmm1
	pxor    %xmm3, %xmm1

	movdqa  %xmm1, %xmm5
	pclmulqdq $0x00, CONSTANT, %xmm1
	pclmulqdq $0x11, CONSTANT, %xmm5
	pxor    %xmm5, %xmm1
	pxor    %xmm4, %xmm1

	cmp     $0x10, LEN
	jb      .Lfold_64
.Lloop_16:
	movdqa  %xmm1, %xmm5
	pclmulqdq $0x00, CONSTANT, %xmm1
	pclmulqdq $0x11, CONSTANT, %xmm5
	pxor    %xmm5, %xmm1
	pxor    (BUF), %xmm1
	sub     $0x10, LEN
	add     $0x10, BUF
	cmp     $0x10, LEN
	jge     .Lloop_16

.Lfold_64:
	
	pclmulqdq $0x01, %xmm1, CONSTANT 
	psrldq  $0x08, %xmm1
	pxor    CONSTANT, %xmm1

	
	movdqa  %xmm1, %xmm2
#ifdef __x86_64__
	movdqa  .Lconstant_R5(%rip), CONSTANT
	movdqa  .Lconstant_mask32(%rip), %xmm3
#else
	movdqa  .Lconstant_R5, CONSTANT
	movdqa  .Lconstant_mask32, %xmm3
#endif
	psrldq  $0x04, %xmm2
	pand    %xmm3, %xmm1
	pclmulqdq $0x00, CONSTANT, %xmm1
	pxor    %xmm2, %xmm1

	
#ifdef __x86_64__
	movdqa  .Lconstant_RUpoly(%rip), CONSTANT
#else
	movdqa  .Lconstant_RUpoly, CONSTANT
#endif
	movdqa  %xmm1, %xmm2
	pand    %xmm3, %xmm1
	pclmulqdq $0x10, CONSTANT, %xmm1
	pand    %xmm3, %xmm1
	pclmulqdq $0x00, CONSTANT, %xmm1
	pxor    %xmm2, %xmm1
	pextrd  $0x01, %xmm1, %eax

	RET
SYM_FUNC_END(crc32_pclmul_le_16)
