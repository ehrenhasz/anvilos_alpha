

#include <linux/linkage.h>
#include <linux/cfi_types.h>

#define DIGEST_PTR	%rdi	
#define DATA_PTR	%rsi	
#define NUM_BLKS	%rdx	

#define SHA256CONSTANTS	%rax

#define MSG		%xmm0
#define STATE0		%xmm1
#define STATE1		%xmm2
#define MSGTMP0		%xmm3
#define MSGTMP1		%xmm4
#define MSGTMP2		%xmm5
#define MSGTMP3		%xmm6
#define MSGTMP4		%xmm7

#define SHUF_MASK	%xmm8

#define ABEF_SAVE	%xmm9
#define CDGH_SAVE	%xmm10



.text
SYM_TYPED_FUNC_START(sha256_ni_transform)

	shl		$6, NUM_BLKS		
	jz		.Ldone_hash
	add		DATA_PTR, NUM_BLKS	

	
	movdqu		0*16(DIGEST_PTR), STATE0
	movdqu		1*16(DIGEST_PTR), STATE1

	pshufd		$0xB1, STATE0,  STATE0		
	pshufd		$0x1B, STATE1,  STATE1		
	movdqa		STATE0, MSGTMP4
	palignr		$8, STATE1,  STATE0		
	pblendw		$0xF0, MSGTMP4, STATE1		

	movdqa		PSHUFFLE_BYTE_FLIP_MASK(%rip), SHUF_MASK
	lea		K256(%rip), SHA256CONSTANTS

.Lloop0:
	
	movdqa		STATE0, ABEF_SAVE
	movdqa		STATE1, CDGH_SAVE

	
	movdqu		0*16(DATA_PTR), MSG
	pshufb		SHUF_MASK, MSG
	movdqa		MSG, MSGTMP0
		paddd		0*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0

	
	movdqu		1*16(DATA_PTR), MSG
	pshufb		SHUF_MASK, MSG
	movdqa		MSG, MSGTMP1
		paddd		1*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP1, MSGTMP0

	
	movdqu		2*16(DATA_PTR), MSG
	pshufb		SHUF_MASK, MSG
	movdqa		MSG, MSGTMP2
		paddd		2*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP2, MSGTMP1

	
	movdqu		3*16(DATA_PTR), MSG
	pshufb		SHUF_MASK, MSG
	movdqa		MSG, MSGTMP3
		paddd		3*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP3, MSGTMP4
	palignr		$4, MSGTMP2, MSGTMP4
	paddd		MSGTMP4, MSGTMP0
	sha256msg2	MSGTMP3, MSGTMP0
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP3, MSGTMP2

	
	movdqa		MSGTMP0, MSG
		paddd		4*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP0, MSGTMP4
	palignr		$4, MSGTMP3, MSGTMP4
	paddd		MSGTMP4, MSGTMP1
	sha256msg2	MSGTMP0, MSGTMP1
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP0, MSGTMP3

	
	movdqa		MSGTMP1, MSG
		paddd		5*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP1, MSGTMP4
	palignr		$4, MSGTMP0, MSGTMP4
	paddd		MSGTMP4, MSGTMP2
	sha256msg2	MSGTMP1, MSGTMP2
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP1, MSGTMP0

	
	movdqa		MSGTMP2, MSG
		paddd		6*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP2, MSGTMP4
	palignr		$4, MSGTMP1, MSGTMP4
	paddd		MSGTMP4, MSGTMP3
	sha256msg2	MSGTMP2, MSGTMP3
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP2, MSGTMP1

	
	movdqa		MSGTMP3, MSG
		paddd		7*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP3, MSGTMP4
	palignr		$4, MSGTMP2, MSGTMP4
	paddd		MSGTMP4, MSGTMP0
	sha256msg2	MSGTMP3, MSGTMP0
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP3, MSGTMP2

	
	movdqa		MSGTMP0, MSG
		paddd		8*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP0, MSGTMP4
	palignr		$4, MSGTMP3, MSGTMP4
	paddd		MSGTMP4, MSGTMP1
	sha256msg2	MSGTMP0, MSGTMP1
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP0, MSGTMP3

	
	movdqa		MSGTMP1, MSG
		paddd		9*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP1, MSGTMP4
	palignr		$4, MSGTMP0, MSGTMP4
	paddd		MSGTMP4, MSGTMP2
	sha256msg2	MSGTMP1, MSGTMP2
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP1, MSGTMP0

	
	movdqa		MSGTMP2, MSG
		paddd		10*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP2, MSGTMP4
	palignr		$4, MSGTMP1, MSGTMP4
	paddd		MSGTMP4, MSGTMP3
	sha256msg2	MSGTMP2, MSGTMP3
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP2, MSGTMP1

	
	movdqa		MSGTMP3, MSG
		paddd		11*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP3, MSGTMP4
	palignr		$4, MSGTMP2, MSGTMP4
	paddd		MSGTMP4, MSGTMP0
	sha256msg2	MSGTMP3, MSGTMP0
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP3, MSGTMP2

	
	movdqa		MSGTMP0, MSG
		paddd		12*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP0, MSGTMP4
	palignr		$4, MSGTMP3, MSGTMP4
	paddd		MSGTMP4, MSGTMP1
	sha256msg2	MSGTMP0, MSGTMP1
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0
	sha256msg1	MSGTMP0, MSGTMP3

	
	movdqa		MSGTMP1, MSG
		paddd		13*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP1, MSGTMP4
	palignr		$4, MSGTMP0, MSGTMP4
	paddd		MSGTMP4, MSGTMP2
	sha256msg2	MSGTMP1, MSGTMP2
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0

	
	movdqa		MSGTMP2, MSG
		paddd		14*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
	movdqa		MSGTMP2, MSGTMP4
	palignr		$4, MSGTMP1, MSGTMP4
	paddd		MSGTMP4, MSGTMP3
	sha256msg2	MSGTMP2, MSGTMP3
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0

	
	movdqa		MSGTMP3, MSG
		paddd		15*16(SHA256CONSTANTS), MSG
		sha256rnds2	STATE0, STATE1
		pshufd 		$0x0E, MSG, MSG
		sha256rnds2	STATE1, STATE0

	
	paddd		ABEF_SAVE, STATE0
	paddd		CDGH_SAVE, STATE1

	
	add		$64, DATA_PTR
	cmp		NUM_BLKS, DATA_PTR
	jne		.Lloop0

	
	pshufd		$0x1B, STATE0,  STATE0		
	pshufd		$0xB1, STATE1,  STATE1		
	movdqa		STATE0, MSGTMP4
	pblendw		$0xF0, STATE1,  STATE0		
	palignr		$8, MSGTMP4, STATE1		

	movdqu		STATE0, 0*16(DIGEST_PTR)
	movdqu		STATE1, 1*16(DIGEST_PTR)

.Ldone_hash:

	RET
SYM_FUNC_END(sha256_ni_transform)

.section	.rodata.cst256.K256, "aM", @progbits, 256
.align 64
K256:
	.long	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5
	.long	0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5
	.long	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3
	.long	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174
	.long	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc
	.long	0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da
	.long	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7
	.long	0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967
	.long	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13
	.long	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85
	.long	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3
	.long	0xd192e819,0xd6990624,0xf40e3585,0x106aa070
	.long	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5
	.long	0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3
	.long	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208
	.long	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2

.section	.rodata.cst16.PSHUFFLE_BYTE_FLIP_MASK, "aM", @progbits, 16
.align 16
PSHUFFLE_BYTE_FLIP_MASK:
	.octa 0x0c0d0e0f08090a0b0405060700010203
