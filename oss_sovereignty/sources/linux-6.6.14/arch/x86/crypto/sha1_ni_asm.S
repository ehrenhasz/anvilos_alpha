

#include <linux/linkage.h>
#include <linux/cfi_types.h>

#define DIGEST_PTR	%rdi	
#define DATA_PTR	%rsi	
#define NUM_BLKS	%rdx	


#define FRAME_SIZE	32	

#define ABCD		%xmm0
#define E0		%xmm1	
#define E1		%xmm2
#define MSG0		%xmm3
#define MSG1		%xmm4
#define MSG2		%xmm5
#define MSG3		%xmm6
#define SHUF_MASK	%xmm7



.text
SYM_TYPED_FUNC_START(sha1_ni_transform)
	push		%rbp
	mov		%rsp, %rbp
	sub		$FRAME_SIZE, %rsp
	and		$~0xF, %rsp

	shl		$6, NUM_BLKS		
	jz		.Ldone_hash
	add		DATA_PTR, NUM_BLKS	

	
	pinsrd		$3, 1*16(DIGEST_PTR), E0
	movdqu		0*16(DIGEST_PTR), ABCD
	pand		UPPER_WORD_MASK(%rip), E0
	pshufd		$0x1B, ABCD, ABCD

	movdqa		PSHUFFLE_BYTE_FLIP_MASK(%rip), SHUF_MASK

.Lloop0:
	
	movdqa		E0, (0*16)(%rsp)
	movdqa		ABCD, (1*16)(%rsp)

	
	movdqu		0*16(DATA_PTR), MSG0
	pshufb		SHUF_MASK, MSG0
		paddd		MSG0, E0
		movdqa		ABCD, E1
		sha1rnds4	$0, E0, ABCD

	
	movdqu		1*16(DATA_PTR), MSG1
	pshufb		SHUF_MASK, MSG1
		sha1nexte	MSG1, E1
		movdqa		ABCD, E0
		sha1rnds4	$0, E1, ABCD
	sha1msg1	MSG1, MSG0

	
	movdqu		2*16(DATA_PTR), MSG2
	pshufb		SHUF_MASK, MSG2
		sha1nexte	MSG2, E0
		movdqa		ABCD, E1
		sha1rnds4	$0, E0, ABCD
	sha1msg1	MSG2, MSG1
	pxor		MSG2, MSG0

	
	movdqu		3*16(DATA_PTR), MSG3
	pshufb		SHUF_MASK, MSG3
		sha1nexte	MSG3, E1
		movdqa		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$0, E1, ABCD
	sha1msg1	MSG3, MSG2
	pxor		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		movdqa		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$0, E0, ABCD
	sha1msg1	MSG0, MSG3
	pxor		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		movdqa		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$1, E1, ABCD
	sha1msg1	MSG1, MSG0
	pxor		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		movdqa		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$1, E0, ABCD
	sha1msg1	MSG2, MSG1
	pxor		MSG2, MSG0

	
		sha1nexte	MSG3, E1
		movdqa		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$1, E1, ABCD
	sha1msg1	MSG3, MSG2
	pxor		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		movdqa		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$1, E0, ABCD
	sha1msg1	MSG0, MSG3
	pxor		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		movdqa		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$1, E1, ABCD
	sha1msg1	MSG1, MSG0
	pxor		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		movdqa		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$2, E0, ABCD
	sha1msg1	MSG2, MSG1
	pxor		MSG2, MSG0

	
		sha1nexte	MSG3, E1
		movdqa		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$2, E1, ABCD
	sha1msg1	MSG3, MSG2
	pxor		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		movdqa		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$2, E0, ABCD
	sha1msg1	MSG0, MSG3
	pxor		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		movdqa		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$2, E1, ABCD
	sha1msg1	MSG1, MSG0
	pxor		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		movdqa		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$2, E0, ABCD
	sha1msg1	MSG2, MSG1
	pxor		MSG2, MSG0

	
		sha1nexte	MSG3, E1
		movdqa		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$3, E1, ABCD
	sha1msg1	MSG3, MSG2
	pxor		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		movdqa		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$3, E0, ABCD
	sha1msg1	MSG0, MSG3
	pxor		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		movdqa		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$3, E1, ABCD
	pxor		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		movdqa		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$3, E0, ABCD

	
		sha1nexte	MSG3, E1
		movdqa		ABCD, E0
		sha1rnds4	$3, E1, ABCD

	
	sha1nexte	(0*16)(%rsp), E0
	paddd		(1*16)(%rsp), ABCD

	
	add		$64, DATA_PTR
	cmp		NUM_BLKS, DATA_PTR
	jne		.Lloop0

	
	pshufd		$0x1B, ABCD, ABCD
	movdqu		ABCD, 0*16(DIGEST_PTR)
	pextrd		$3, E0, 1*16(DIGEST_PTR)

.Ldone_hash:
	mov		%rbp, %rsp
	pop		%rbp

	RET
SYM_FUNC_END(sha1_ni_transform)

.section	.rodata.cst16.PSHUFFLE_BYTE_FLIP_MASK, "aM", @progbits, 16
.align 16
PSHUFFLE_BYTE_FLIP_MASK:
	.octa 0x000102030405060708090a0b0c0d0e0f

.section	.rodata.cst16.UPPER_WORD_MASK, "aM", @progbits, 16
.align 16
UPPER_WORD_MASK:
	.octa 0xFFFFFFFF000000000000000000000000
