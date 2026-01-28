#if ENABLE_SHA1_HWACCEL && defined(__GNUC__) && defined(__x86_64__)










#define mova128 movaps

#define movu128 movups

#define xor128 xorps

#define shuf128_32 shufps

#define extr128_32 pextrd







#ifdef __linux__
	.section	.note.GNU-stack, "", @progbits
#endif
	.section	.text.sha1_process_block64_shaNI, "ax", @progbits
	.globl	sha1_process_block64_shaNI
	.hidden	sha1_process_block64_shaNI
	.type	sha1_process_block64_shaNI, @function

#define ABCD		%xmm0
#define E0		%xmm1	
#define E1		%xmm2
#define MSG0		%xmm3
#define MSG1		%xmm4
#define MSG2		%xmm5
#define MSG3		%xmm6

	.balign	8	# allow decoders to fetch at least 2 first insns
sha1_process_block64_shaNI:
	
	movu128		80(%rdi), ABCD
	xor128		E0, E0
	pinsrd		$3, 80+4*4(%rdi), E0	# load to uppermost 32-bit word
	shuf128_32	$0x1B, ABCD, ABCD	# DCBA -> ABCD

	mova128		PSHUFFLE_BYTE_FLIP_MASK(%rip), %xmm7

	movu128		0*16(%rdi), MSG0
	pshufb		%xmm7, MSG0
	movu128		1*16(%rdi), MSG1
	pshufb		%xmm7, MSG1
	movu128		2*16(%rdi), MSG2
	pshufb		%xmm7, MSG2
	movu128		3*16(%rdi), MSG3
	pshufb		%xmm7, MSG3

	
	mova128		E0, %xmm7
	mova128		ABCD, %xmm8

	
		paddd		MSG0, E0
		mova128		ABCD, E1
		sha1rnds4	$0, E0, ABCD

	
		sha1nexte	MSG1, E1
		mova128		ABCD, E0
		sha1rnds4	$0, E1, ABCD
	sha1msg1	MSG1, MSG0

	
		sha1nexte	MSG2, E0
		mova128		ABCD, E1
		sha1rnds4	$0, E0, ABCD
	sha1msg1	MSG2, MSG1
	xor128		MSG2, MSG0

	
		sha1nexte	MSG3, E1
		mova128		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$0, E1, ABCD
	sha1msg1	MSG3, MSG2
	xor128		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		mova128		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$0, E0, ABCD
	sha1msg1	MSG0, MSG3
	xor128		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		mova128		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$1, E1, ABCD
	sha1msg1	MSG1, MSG0
	xor128		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		mova128		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$1, E0, ABCD
	sha1msg1	MSG2, MSG1
	xor128		MSG2, MSG0

	
		sha1nexte	MSG3, E1
		mova128		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$1, E1, ABCD
	sha1msg1	MSG3, MSG2
	xor128		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		mova128		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$1, E0, ABCD
	sha1msg1	MSG0, MSG3
	xor128		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		mova128		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$1, E1, ABCD
	sha1msg1	MSG1, MSG0
	xor128		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		mova128		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$2, E0, ABCD
	sha1msg1	MSG2, MSG1
	xor128		MSG2, MSG0

	
		sha1nexte	MSG3, E1
		mova128		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$2, E1, ABCD
	sha1msg1	MSG3, MSG2
	xor128		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		mova128		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$2, E0, ABCD
	sha1msg1	MSG0, MSG3
	xor128		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		mova128		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$2, E1, ABCD
	sha1msg1	MSG1, MSG0
	xor128		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		mova128		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$2, E0, ABCD
	sha1msg1	MSG2, MSG1
	xor128		MSG2, MSG0

	
		sha1nexte	MSG3, E1
		mova128		ABCD, E0
	sha1msg2	MSG3, MSG0
		sha1rnds4	$3, E1, ABCD
	sha1msg1	MSG3, MSG2
	xor128		MSG3, MSG1

	
		sha1nexte	MSG0, E0
		mova128		ABCD, E1
	sha1msg2	MSG0, MSG1
		sha1rnds4	$3, E0, ABCD
	sha1msg1	MSG0, MSG3
	xor128		MSG0, MSG2

	
		sha1nexte	MSG1, E1
		mova128		ABCD, E0
	sha1msg2	MSG1, MSG2
		sha1rnds4	$3, E1, ABCD
	xor128		MSG1, MSG3

	
		sha1nexte	MSG2, E0
		mova128		ABCD, E1
	sha1msg2	MSG2, MSG3
		sha1rnds4	$3, E0, ABCD

	
		sha1nexte	MSG3, E1
		mova128		ABCD, E0
		sha1rnds4	$3, E1, ABCD

	
	sha1nexte	%xmm7, E0
	paddd		%xmm8, ABCD

	
	shuf128_32	$0x1B, ABCD, ABCD
	movu128		ABCD, 80(%rdi)
	extr128_32	$3, E0, 80+4*4(%rdi)

	ret
	.size	sha1_process_block64_shaNI, .-sha1_process_block64_shaNI

	.section	.rodata.cst16.PSHUFFLE_BYTE_FLIP_MASK, "aM", @progbits, 16
	.balign	16
PSHUFFLE_BYTE_FLIP_MASK:
	.octa	0x000102030405060708090a0b0c0d0e0f

#endif
