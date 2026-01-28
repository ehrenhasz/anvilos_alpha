#ifndef _ASM_S390_CPACF_H
#define _ASM_S390_CPACF_H
#include <asm/facility.h>
#define CPACF_KMAC		0xb91e		 
#define CPACF_KM		0xb92e		 
#define CPACF_KMC		0xb92f		 
#define CPACF_KIMD		0xb93e		 
#define CPACF_KLMD		0xb93f		 
#define CPACF_PCKMO		0xb928		 
#define CPACF_KMF		0xb92a		 
#define CPACF_KMO		0xb92b		 
#define CPACF_PCC		0xb92c		 
#define CPACF_KMCTR		0xb92d		 
#define CPACF_PRNO		0xb93c		 
#define CPACF_KMA		0xb929		 
#define CPACF_KDSA		0xb93a		 
#define CPACF_ENCRYPT		0x00
#define CPACF_DECRYPT		0x80
#define CPACF_KM_QUERY		0x00
#define CPACF_KM_DEA		0x01
#define CPACF_KM_TDEA_128	0x02
#define CPACF_KM_TDEA_192	0x03
#define CPACF_KM_AES_128	0x12
#define CPACF_KM_AES_192	0x13
#define CPACF_KM_AES_256	0x14
#define CPACF_KM_PAES_128	0x1a
#define CPACF_KM_PAES_192	0x1b
#define CPACF_KM_PAES_256	0x1c
#define CPACF_KM_XTS_128	0x32
#define CPACF_KM_XTS_256	0x34
#define CPACF_KM_PXTS_128	0x3a
#define CPACF_KM_PXTS_256	0x3c
#define CPACF_KMC_QUERY		0x00
#define CPACF_KMC_DEA		0x01
#define CPACF_KMC_TDEA_128	0x02
#define CPACF_KMC_TDEA_192	0x03
#define CPACF_KMC_AES_128	0x12
#define CPACF_KMC_AES_192	0x13
#define CPACF_KMC_AES_256	0x14
#define CPACF_KMC_PAES_128	0x1a
#define CPACF_KMC_PAES_192	0x1b
#define CPACF_KMC_PAES_256	0x1c
#define CPACF_KMC_PRNG		0x43
#define CPACF_KMCTR_QUERY	0x00
#define CPACF_KMCTR_DEA		0x01
#define CPACF_KMCTR_TDEA_128	0x02
#define CPACF_KMCTR_TDEA_192	0x03
#define CPACF_KMCTR_AES_128	0x12
#define CPACF_KMCTR_AES_192	0x13
#define CPACF_KMCTR_AES_256	0x14
#define CPACF_KMCTR_PAES_128	0x1a
#define CPACF_KMCTR_PAES_192	0x1b
#define CPACF_KMCTR_PAES_256	0x1c
#define CPACF_KIMD_QUERY	0x00
#define CPACF_KIMD_SHA_1	0x01
#define CPACF_KIMD_SHA_256	0x02
#define CPACF_KIMD_SHA_512	0x03
#define CPACF_KIMD_SHA3_224	0x20
#define CPACF_KIMD_SHA3_256	0x21
#define CPACF_KIMD_SHA3_384	0x22
#define CPACF_KIMD_SHA3_512	0x23
#define CPACF_KIMD_GHASH	0x41
#define CPACF_KLMD_QUERY	0x00
#define CPACF_KLMD_SHA_1	0x01
#define CPACF_KLMD_SHA_256	0x02
#define CPACF_KLMD_SHA_512	0x03
#define CPACF_KLMD_SHA3_224	0x20
#define CPACF_KLMD_SHA3_256	0x21
#define CPACF_KLMD_SHA3_384	0x22
#define CPACF_KLMD_SHA3_512	0x23
#define CPACF_KMAC_QUERY	0x00
#define CPACF_KMAC_DEA		0x01
#define CPACF_KMAC_TDEA_128	0x02
#define CPACF_KMAC_TDEA_192	0x03
#define CPACF_PCKMO_QUERY		0x00
#define CPACF_PCKMO_ENC_DES_KEY		0x01
#define CPACF_PCKMO_ENC_TDES_128_KEY	0x02
#define CPACF_PCKMO_ENC_TDES_192_KEY	0x03
#define CPACF_PCKMO_ENC_AES_128_KEY	0x12
#define CPACF_PCKMO_ENC_AES_192_KEY	0x13
#define CPACF_PCKMO_ENC_AES_256_KEY	0x14
#define CPACF_PCKMO_ENC_ECC_P256_KEY	0x20
#define CPACF_PCKMO_ENC_ECC_P384_KEY	0x21
#define CPACF_PCKMO_ENC_ECC_P521_KEY	0x22
#define CPACF_PCKMO_ENC_ECC_ED25519_KEY	0x28
#define CPACF_PCKMO_ENC_ECC_ED448_KEY	0x29
#define CPACF_PRNO_QUERY		0x00
#define CPACF_PRNO_SHA512_DRNG_GEN	0x03
#define CPACF_PRNO_SHA512_DRNG_SEED	0x83
#define CPACF_PRNO_TRNG_Q_R2C_RATIO	0x70
#define CPACF_PRNO_TRNG			0x72
#define CPACF_KMA_QUERY		0x00
#define CPACF_KMA_GCM_AES_128	0x12
#define CPACF_KMA_GCM_AES_192	0x13
#define CPACF_KMA_GCM_AES_256	0x14
#define CPACF_KMA_LPC	0x100	 
#define CPACF_KMA_LAAD	0x200	 
#define CPACF_KMA_HS	0x400	 
typedef struct { unsigned char bytes[16]; } cpacf_mask_t;
static __always_inline void __cpacf_query(unsigned int opcode, cpacf_mask_t *mask)
{
	asm volatile(
		"	lghi	0,0\n"  
		"	lgr	1,%[mask]\n"
		"	spm	0\n"  
		"0:	.insn	rrf,%[opc] << 16,2,4,6,0\n"
		"	brc	1,0b\n"	 
		: "=m" (*mask)
		: [mask] "d" ((unsigned long)mask), [opc] "i" (opcode)
		: "cc", "0", "1");
}
static __always_inline int __cpacf_check_opcode(unsigned int opcode)
{
	switch (opcode) {
	case CPACF_KMAC:
	case CPACF_KM:
	case CPACF_KMC:
	case CPACF_KIMD:
	case CPACF_KLMD:
		return test_facility(17);	 
	case CPACF_PCKMO:
		return test_facility(76);	 
	case CPACF_KMF:
	case CPACF_KMO:
	case CPACF_PCC:
	case CPACF_KMCTR:
		return test_facility(77);	 
	case CPACF_PRNO:
		return test_facility(57);	 
	case CPACF_KMA:
		return test_facility(146);	 
	default:
		BUG();
	}
}
static __always_inline int cpacf_query(unsigned int opcode, cpacf_mask_t *mask)
{
	if (__cpacf_check_opcode(opcode)) {
		__cpacf_query(opcode, mask);
		return 1;
	}
	memset(mask, 0, sizeof(*mask));
	return 0;
}
static inline int cpacf_test_func(cpacf_mask_t *mask, unsigned int func)
{
	return (mask->bytes[func >> 3] & (0x80 >> (func & 7))) != 0;
}
static __always_inline int cpacf_query_func(unsigned int opcode, unsigned int func)
{
	cpacf_mask_t mask;
	if (cpacf_query(opcode, &mask))
		return cpacf_test_func(&mask, func);
	return 0;
}
static inline int cpacf_km(unsigned long func, void *param,
			   u8 *dest, const u8 *src, long src_len)
{
	union register_pair d, s;
	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,%[dst],%[src]\n"
		"	brc	1,0b\n"  
		: [src] "+&d" (s.pair), [dst] "+&d" (d.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KM)
		: "cc", "memory", "0", "1");
	return src_len - s.odd;
}
static inline int cpacf_kmc(unsigned long func, void *param,
			    u8 *dest, const u8 *src, long src_len)
{
	union register_pair d, s;
	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,%[dst],%[src]\n"
		"	brc	1,0b\n"  
		: [src] "+&d" (s.pair), [dst] "+&d" (d.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMC)
		: "cc", "memory", "0", "1");
	return src_len - s.odd;
}
static inline void cpacf_kimd(unsigned long func, void *param,
			      const u8 *src, long src_len)
{
	union register_pair s;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,0,%[src]\n"
		"	brc	1,0b\n"  
		: [src] "+&d" (s.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)(param)),
		  [opc] "i" (CPACF_KIMD)
		: "cc", "memory", "0", "1");
}
static inline void cpacf_klmd(unsigned long func, void *param,
			      const u8 *src, long src_len)
{
	union register_pair s;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,0,%[src]\n"
		"	brc	1,0b\n"  
		: [src] "+&d" (s.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KLMD)
		: "cc", "memory", "0", "1");
}
static inline int cpacf_kmac(unsigned long func, void *param,
			     const u8 *src, long src_len)
{
	union register_pair s;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,0,%[src]\n"
		"	brc	1,0b\n"  
		: [src] "+&d" (s.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMAC)
		: "cc", "memory", "0", "1");
	return src_len - s.odd;
}
static inline int cpacf_kmctr(unsigned long func, void *param, u8 *dest,
			      const u8 *src, long src_len, u8 *counter)
{
	union register_pair d, s, c;
	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	c.even = (unsigned long)counter;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rrf,%[opc] << 16,%[dst],%[src],%[ctr],0\n"
		"	brc	1,0b\n"  
		: [src] "+&d" (s.pair), [dst] "+&d" (d.pair),
		  [ctr] "+&d" (c.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMCTR)
		: "cc", "memory", "0", "1");
	return src_len - s.odd;
}
static inline void cpacf_prno(unsigned long func, void *param,
			      u8 *dest, unsigned long dest_len,
			      const u8 *seed, unsigned long seed_len)
{
	union register_pair d, s;
	d.even = (unsigned long)dest;
	d.odd  = (unsigned long)dest_len;
	s.even = (unsigned long)seed;
	s.odd  = (unsigned long)seed_len;
	asm volatile (
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,%[dst],%[seed]\n"
		"	brc	1,0b\n"	   
		: [dst] "+&d" (d.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [seed] "d" (s.pair), [opc] "i" (CPACF_PRNO)
		: "cc", "memory", "0", "1");
}
static inline void cpacf_trng(u8 *ucbuf, unsigned long ucbuf_len,
			      u8 *cbuf, unsigned long cbuf_len)
{
	union register_pair u, c;
	u.even = (unsigned long)ucbuf;
	u.odd  = (unsigned long)ucbuf_len;
	c.even = (unsigned long)cbuf;
	c.odd  = (unsigned long)cbuf_len;
	asm volatile (
		"	lghi	0,%[fc]\n"
		"0:	.insn	rre,%[opc] << 16,%[ucbuf],%[cbuf]\n"
		"	brc	1,0b\n"	   
		: [ucbuf] "+&d" (u.pair), [cbuf] "+&d" (c.pair)
		: [fc] "K" (CPACF_PRNO_TRNG), [opc] "i" (CPACF_PRNO)
		: "cc", "memory", "0");
}
static inline void cpacf_pcc(unsigned long func, void *param)
{
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rre,%[opc] << 16,0,0\n"  
		"	brc	1,0b\n"  
		:
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_PCC)
		: "cc", "memory", "0", "1");
}
static inline void cpacf_pckmo(long func, void *param)
{
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"       .insn   rre,%[opc] << 16,0,0\n"  
		:
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_PCKMO)
		: "cc", "memory", "0", "1");
}
static inline void cpacf_kma(unsigned long func, void *param, u8 *dest,
			     const u8 *src, unsigned long src_len,
			     const u8 *aad, unsigned long aad_len)
{
	union register_pair d, s, a;
	d.even = (unsigned long)dest;
	s.even = (unsigned long)src;
	s.odd  = (unsigned long)src_len;
	a.even = (unsigned long)aad;
	a.odd  = (unsigned long)aad_len;
	asm volatile(
		"	lgr	0,%[fc]\n"
		"	lgr	1,%[pba]\n"
		"0:	.insn	rrf,%[opc] << 16,%[dst],%[src],%[aad],0\n"
		"	brc	1,0b\n"	 
		: [dst] "+&d" (d.pair), [src] "+&d" (s.pair),
		  [aad] "+&d" (a.pair)
		: [fc] "d" (func), [pba] "d" ((unsigned long)param),
		  [opc] "i" (CPACF_KMA)
		: "cc", "memory", "0", "1");
}
#endif	 
