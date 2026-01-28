#ifndef _AESOPT_H
#define	_AESOPT_H
#ifdef	__cplusplus
extern "C" {
#endif
#include <sys/zfs_context.h>
#include <aes/aes_impl.h>
#define	AES_ENCRYPT  
#define	AES_DECRYPT  
#define	IS_BIG_ENDIAN		4321  
#define	IS_LITTLE_ENDIAN	1234  
#define	PLATFORM_BYTE_ORDER	IS_LITTLE_ENDIAN
#define	AES_REV_DKS  
#define	ENCRYPTION_IN_C	1
#define	DECRYPTION_IN_C	2
#define	ENC_KEYING_IN_C	4
#define	DEC_KEYING_IN_C	8
#define	NO_TABLES	0
#define	ONE_TABLE	1
#define	FOUR_TABLES	4
#define	NONE		0
#define	PARTIAL		1
#define	FULL		2
#if 1
#define	ALGORITHM_BYTE_ORDER PLATFORM_BYTE_ORDER
#elif 0
#define	ALGORITHM_BYTE_ORDER IS_LITTLE_ENDIAN
#elif 0
#define	ALGORITHM_BYTE_ORDER IS_BIG_ENDIAN
#else
#error The algorithm byte order is not defined
#endif
#if defined(__GNUC__) && defined(__i386__) || \
	defined(_WIN32) && defined(_M_IX86) && \
	!(defined(_WIN64) || defined(_WIN32_WCE) || \
	defined(_MSC_VER) && (_MSC_VER <= 800))
#define	VIA_ACE_POSSIBLE
#endif
#undef	VIA_ACE_POSSIBLE
#undef	ASSUME_VIA_ACE_PRESENT
#if 0 && defined(VIA_ACE_POSSIBLE) && !defined(USE_VIA_ACE_IF_PRESENT)
#define	USE_VIA_ACE_IF_PRESENT
#endif
#if 0 && defined(VIA_ACE_POSSIBLE) && !defined(ASSUME_VIA_ACE_PRESENT)
#define	ASSUME_VIA_ACE_PRESENT
#endif
#if 0 && !defined(ASM_X86_V1C)
#define	ASM_X86_V1C
#elif 0 && !defined(ASM_X86_V2)
#define	ASM_X86_V2
#elif 0 && !defined(ASM_X86_V2C)
#define	ASM_X86_V2C
#elif 1 && !defined(ASM_AMD64_C)
#define	ASM_AMD64_C
#endif
#if (defined(ASM_X86_V1C) || defined(ASM_X86_V2) || defined(ASM_X86_V2C)) && \
	!defined(_M_IX86) || defined(ASM_AMD64_C) && !defined(_M_X64) && \
	!defined(__amd64)
#error Assembler code is only available for x86 and AMD64 systems
#endif
#if 1 && !defined(_MSC_VER)
#define	SAFE_IO
#endif
#if 1
#define	ENC_UNROLL  FULL
#elif 0
#define	ENC_UNROLL  PARTIAL
#else
#define	ENC_UNROLL  NONE
#endif
#if 1
#define	DEC_UNROLL  FULL
#elif 0
#define	DEC_UNROLL  PARTIAL
#else
#define	DEC_UNROLL  NONE
#endif
#if 1
#define	ENC_KS_UNROLL
#endif
#if 1
#define	DEC_KS_UNROLL
#endif
#if 1
#define	FF_TABLES
#endif
#if 1
#define	ARRAYS
#endif
#if 1 && !(defined(_MSC_VER) && (_MSC_VER <= 800))
#define	FIXED_TABLES
#endif
#if 0
#define	to_byte(x)  ((uint8_t)(x))
#else
#define	to_byte(x)  ((x) & 0xff)
#endif
#if 1 && defined(_MSC_VER) && (_MSC_VER >= 1300)
#define	TABLE_ALIGN 32
#endif
#if 1 && (defined(ASM_X86_V2) || defined(ASM_X86_V2C))
#define	REDUCE_CODE_SIZE
#endif
#if 1    
#define	ENC_ROUND   FOUR_TABLES
#elif 0
#define	ENC_ROUND   ONE_TABLE
#else
#define	ENC_ROUND   NO_TABLES
#endif
#if 1    
#define	LAST_ENC_ROUND  FOUR_TABLES
#elif 0
#define	LAST_ENC_ROUND  ONE_TABLE
#else
#define	LAST_ENC_ROUND  NO_TABLES
#endif
#if 1    
#define	DEC_ROUND   FOUR_TABLES
#elif 0
#define	DEC_ROUND   ONE_TABLE
#else
#define	DEC_ROUND   NO_TABLES
#endif
#if 1    
#define	LAST_DEC_ROUND  FOUR_TABLES
#elif 0
#define	LAST_DEC_ROUND  ONE_TABLE
#else
#define	LAST_DEC_ROUND  NO_TABLES
#endif
#if 1
#define	KEY_SCHED   FOUR_TABLES
#elif 0
#define	KEY_SCHED   ONE_TABLE
#else
#define	KEY_SCHED   NO_TABLES
#endif
#if !defined(_MSC_VER) && !defined(__GNUC__)
#if defined(ASSUME_VIA_ACE_PRESENT)
#undef ASSUME_VIA_ACE_PRESENT
#endif
#if defined(USE_VIA_ACE_IF_PRESENT)
#undef USE_VIA_ACE_IF_PRESENT
#endif
#endif
#if defined(ASSUME_VIA_ACE_PRESENT) && !defined(USE_VIA_ACE_IF_PRESENT)
#define	USE_VIA_ACE_IF_PRESENT
#endif
#if defined(USE_VIA_ACE_IF_PRESENT) && !defined(AES_REV_DKS)
#define	AES_REV_DKS
#endif
#if (defined(ASM_X86_V1C) || defined(ASM_X86_V2C) || defined(ASM_AMD64_C)) && \
	(ALGORITHM_BYTE_ORDER != PLATFORM_BYTE_ORDER)
#undef  ALGORITHM_BYTE_ORDER
#define	ALGORITHM_BYTE_ORDER PLATFORM_BYTE_ORDER
#endif
#if defined(ARRAYS)
#define	s(x, c) x[c]
#else
#define	s(x, c) x##c
#endif
#if !defined(AES_ENCRYPT)
#define	EFUNCS_IN_C   0
#elif defined(ASSUME_VIA_ACE_PRESENT) || defined(ASM_X86_V1C) || \
	defined(ASM_X86_V2C) || defined(ASM_AMD64_C)
#define	EFUNCS_IN_C   ENC_KEYING_IN_C
#elif !defined(ASM_X86_V2)
#define	EFUNCS_IN_C   (ENCRYPTION_IN_C | ENC_KEYING_IN_C)
#else
#define	EFUNCS_IN_C   0
#endif
#if !defined(AES_DECRYPT)
#define	DFUNCS_IN_C   0
#elif defined(ASSUME_VIA_ACE_PRESENT) || defined(ASM_X86_V1C) || \
	defined(ASM_X86_V2C) || defined(ASM_AMD64_C)
#define	DFUNCS_IN_C   DEC_KEYING_IN_C
#elif !defined(ASM_X86_V2)
#define	DFUNCS_IN_C   (DECRYPTION_IN_C | DEC_KEYING_IN_C)
#else
#define	DFUNCS_IN_C   0
#endif
#define	FUNCS_IN_C  (EFUNCS_IN_C | DFUNCS_IN_C)
#if ENC_ROUND == NO_TABLES && LAST_ENC_ROUND != NO_TABLES
#undef  LAST_ENC_ROUND
#define	LAST_ENC_ROUND  NO_TABLES
#elif ENC_ROUND == ONE_TABLE && LAST_ENC_ROUND == FOUR_TABLES
#undef  LAST_ENC_ROUND
#define	LAST_ENC_ROUND  ONE_TABLE
#endif
#if ENC_ROUND == NO_TABLES && ENC_UNROLL != NONE
#undef  ENC_UNROLL
#define	ENC_UNROLL  NONE
#endif
#if DEC_ROUND == NO_TABLES && LAST_DEC_ROUND != NO_TABLES
#undef  LAST_DEC_ROUND
#define	LAST_DEC_ROUND  NO_TABLES
#elif DEC_ROUND == ONE_TABLE && LAST_DEC_ROUND == FOUR_TABLES
#undef  LAST_DEC_ROUND
#define	LAST_DEC_ROUND  ONE_TABLE
#endif
#if DEC_ROUND == NO_TABLES && DEC_UNROLL != NONE
#undef  DEC_UNROLL
#define	DEC_UNROLL  NONE
#endif
#if (ALGORITHM_BYTE_ORDER == IS_LITTLE_ENDIAN)
#define	aes_sw32	htonl
#elif defined(bswap32)
#define	aes_sw32	bswap32
#elif defined(bswap_32)
#define	aes_sw32	bswap_32
#else
#define	brot(x, n)  (((uint32_t)(x) << (n)) | ((uint32_t)(x) >> (32 - (n))))
#define	aes_sw32(x) ((brot((x), 8) & 0x00ff00ff) | (brot((x), 24) & 0xff00ff00))
#endif
#if (ALGORITHM_BYTE_ORDER == IS_LITTLE_ENDIAN)
#define	upr(x, n)	(((uint32_t)(x) << (8 * (n))) | \
			((uint32_t)(x) >> (32 - 8 * (n))))
#define	ups(x, n)	((uint32_t)(x) << (8 * (n)))
#define	bval(x, n)	to_byte((x) >> (8 * (n)))
#define	bytes2word(b0, b1, b2, b3)  \
		(((uint32_t)(b3) << 24) | ((uint32_t)(b2) << 16) | \
		((uint32_t)(b1) << 8) | (b0))
#endif
#if (ALGORITHM_BYTE_ORDER == IS_BIG_ENDIAN)
#define	upr(x, n)	(((uint32_t)(x) >> (8 * (n))) | \
			((uint32_t)(x) << (32 - 8 * (n))))
#define	ups(x, n)	((uint32_t)(x) >> (8 * (n)))
#define	bval(x, n)	to_byte((x) >> (24 - 8 * (n)))
#define	bytes2word(b0, b1, b2, b3)  \
		(((uint32_t)(b0) << 24) | ((uint32_t)(b1) << 16) | \
		((uint32_t)(b2) << 8) | (b3))
#endif
#if defined(SAFE_IO)
#define	word_in(x, c)	bytes2word(((const uint8_t *)(x) + 4 * c)[0], \
				((const uint8_t *)(x) + 4 * c)[1], \
				((const uint8_t *)(x) + 4 * c)[2], \
				((const uint8_t *)(x) + 4 * c)[3])
#define	word_out(x, c, v) { ((uint8_t *)(x) + 4 * c)[0] = bval(v, 0); \
			((uint8_t *)(x) + 4 * c)[1] = bval(v, 1); \
			((uint8_t *)(x) + 4 * c)[2] = bval(v, 2); \
			((uint8_t *)(x) + 4 * c)[3] = bval(v, 3); }
#elif (ALGORITHM_BYTE_ORDER == PLATFORM_BYTE_ORDER)
#define	word_in(x, c)	(*((uint32_t *)(x) + (c)))
#define	word_out(x, c, v) (*((uint32_t *)(x) + (c)) = (v))
#else
#define	word_in(x, c)	aes_sw32(*((uint32_t *)(x) + (c)))
#define	word_out(x, c, v) (*((uint32_t *)(x) + (c)) = aes_sw32(v))
#endif
#define	WPOLY   0x011b
#define	BPOLY	0x1b
#define	m1  0x80808080
#define	m2  0x7f7f7f7f
#define	gf_mulx(x)  ((((x) & m2) << 1) ^ ((((x) & m1) >> 7) * BPOLY))
#if defined(ASM_X86_V1C)
#if defined(ENC_ROUND)
#undef  ENC_ROUND
#endif
#define	ENC_ROUND   FOUR_TABLES
#if defined(LAST_ENC_ROUND)
#undef  LAST_ENC_ROUND
#endif
#define	LAST_ENC_ROUND  FOUR_TABLES
#if defined(DEC_ROUND)
#undef  DEC_ROUND
#endif
#define	DEC_ROUND   FOUR_TABLES
#if defined(LAST_DEC_ROUND)
#undef  LAST_DEC_ROUND
#endif
#define	LAST_DEC_ROUND  FOUR_TABLES
#if defined(KEY_SCHED)
#undef  KEY_SCHED
#define	KEY_SCHED   FOUR_TABLES
#endif
#endif
#if (FUNCS_IN_C & ENCRYPTION_IN_C) || defined(ASM_X86_V1C)
#if ENC_ROUND == ONE_TABLE
#define	FT1_SET
#elif ENC_ROUND == FOUR_TABLES
#define	FT4_SET
#else
#define	SBX_SET
#endif
#if LAST_ENC_ROUND == ONE_TABLE
#define	FL1_SET
#elif LAST_ENC_ROUND == FOUR_TABLES
#define	FL4_SET
#elif !defined(SBX_SET)
#define	SBX_SET
#endif
#endif
#if (FUNCS_IN_C & DECRYPTION_IN_C) || defined(ASM_X86_V1C)
#if DEC_ROUND == ONE_TABLE
#define	IT1_SET
#elif DEC_ROUND == FOUR_TABLES
#define	IT4_SET
#else
#define	ISB_SET
#endif
#if LAST_DEC_ROUND == ONE_TABLE
#define	IL1_SET
#elif LAST_DEC_ROUND == FOUR_TABLES
#define	IL4_SET
#elif !defined(ISB_SET)
#define	ISB_SET
#endif
#endif
#if !(defined(REDUCE_CODE_SIZE) && (defined(ASM_X86_V2) || \
	defined(ASM_X86_V2C)))
#if ((FUNCS_IN_C & ENC_KEYING_IN_C) || (FUNCS_IN_C & DEC_KEYING_IN_C))
#if KEY_SCHED == ONE_TABLE
#if !defined(FL1_SET) && !defined(FL4_SET)
#define	LS1_SET
#endif
#elif KEY_SCHED == FOUR_TABLES
#if !defined(FL4_SET)
#define	LS4_SET
#endif
#elif !defined(SBX_SET)
#define	SBX_SET
#endif
#endif
#if (FUNCS_IN_C & DEC_KEYING_IN_C)
#if KEY_SCHED == ONE_TABLE
#define	IM1_SET
#elif KEY_SCHED == FOUR_TABLES
#define	IM4_SET
#elif !defined(SBX_SET)
#define	SBX_SET
#endif
#endif
#endif
#define	no_table(x, box, vf, rf, c) bytes2word(\
	box[bval(vf(x, 0, c), rf(0, c))], \
	box[bval(vf(x, 1, c), rf(1, c))], \
	box[bval(vf(x, 2, c), rf(2, c))], \
	box[bval(vf(x, 3, c), rf(3, c))])
#define	one_table(x, op, tab, vf, rf, c) \
	(tab[bval(vf(x, 0, c), rf(0, c))] \
	^ op(tab[bval(vf(x, 1, c), rf(1, c))], 1) \
	^ op(tab[bval(vf(x, 2, c), rf(2, c))], 2) \
	^ op(tab[bval(vf(x, 3, c), rf(3, c))], 3))
#define	four_tables(x, tab, vf, rf, c) \
	(tab[0][bval(vf(x, 0, c), rf(0, c))] \
	^ tab[1][bval(vf(x, 1, c), rf(1, c))] \
	^ tab[2][bval(vf(x, 2, c), rf(2, c))] \
	^ tab[3][bval(vf(x, 3, c), rf(3, c))])
#define	vf1(x, r, c)	(x)
#define	rf1(r, c)	(r)
#define	rf2(r, c)	((8+r-c)&3)
#if !(defined(REDUCE_CODE_SIZE) && (defined(ASM_X86_V2) || \
	defined(ASM_X86_V2C)))
#if defined(FM4_SET)	 
#define	fwd_mcol(x)	four_tables(x, t_use(f, m), vf1, rf1, 0)
#elif defined(FM1_SET)	 
#define	fwd_mcol(x)	one_table(x, upr, t_use(f, m), vf1, rf1, 0)
#else
#define	dec_fmvars	uint32_t g2
#define	fwd_mcol(x)	(g2 = gf_mulx(x), g2 ^ upr((x) ^ g2, 3) ^ \
				upr((x), 2) ^ upr((x), 1))
#endif
#if defined(IM4_SET)
#define	inv_mcol(x)	four_tables(x, t_use(i, m), vf1, rf1, 0)
#elif defined(IM1_SET)
#define	inv_mcol(x)	one_table(x, upr, t_use(i, m), vf1, rf1, 0)
#else
#define	dec_imvars	uint32_t g2, g4, g9
#define	inv_mcol(x)	(g2 = gf_mulx(x), g4 = gf_mulx(g2), g9 = \
				(x) ^ gf_mulx(g4), g4 ^= g9, \
				(x) ^ g2 ^ g4 ^ upr(g2 ^ g9, 3) ^ \
				upr(g4, 2) ^ upr(g9, 1))
#endif
#if defined(FL4_SET)
#define	ls_box(x, c)	four_tables(x, t_use(f, l), vf1, rf2, c)
#elif defined(LS4_SET)
#define	ls_box(x, c)	four_tables(x, t_use(l, s), vf1, rf2, c)
#elif defined(FL1_SET)
#define	ls_box(x, c)	one_table(x, upr, t_use(f, l), vf1, rf2, c)
#elif defined(LS1_SET)
#define	ls_box(x, c)	one_table(x, upr, t_use(l, s), vf1, rf2, c)
#else
#define	ls_box(x, c)	no_table(x, t_use(s, box), vf1, rf2, c)
#endif
#endif
#if defined(ASM_X86_V1C) && defined(AES_DECRYPT) && !defined(ISB_SET)
#define	ISB_SET
#endif
#ifdef	__cplusplus
}
#endif
#endif	 
