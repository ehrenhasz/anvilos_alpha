


#ifndef	_AES_IMPL_H
#define	_AES_IMPL_H



#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/asm_linkage.h>


#define	IS_P2ALIGNED2(v, w, a) \
	((((uintptr_t)(v) | (uintptr_t)(w)) & ((uintptr_t)(a) - 1)) == 0)

#define	AES_BLOCK_LEN	16	

#define	RC_LENGTH	(5 * ((AES_BLOCK_LEN) / 4 - 2))

#define	AES_COPY_BLOCK(src, dst) \
	(dst)[0] = (src)[0]; \
	(dst)[1] = (src)[1]; \
	(dst)[2] = (src)[2]; \
	(dst)[3] = (src)[3]; \
	(dst)[4] = (src)[4]; \
	(dst)[5] = (src)[5]; \
	(dst)[6] = (src)[6]; \
	(dst)[7] = (src)[7]; \
	(dst)[8] = (src)[8]; \
	(dst)[9] = (src)[9]; \
	(dst)[10] = (src)[10]; \
	(dst)[11] = (src)[11]; \
	(dst)[12] = (src)[12]; \
	(dst)[13] = (src)[13]; \
	(dst)[14] = (src)[14]; \
	(dst)[15] = (src)[15]

#define	AES_XOR_BLOCK(src, dst) \
	(dst)[0] ^= (src)[0]; \
	(dst)[1] ^= (src)[1]; \
	(dst)[2] ^= (src)[2]; \
	(dst)[3] ^= (src)[3]; \
	(dst)[4] ^= (src)[4]; \
	(dst)[5] ^= (src)[5]; \
	(dst)[6] ^= (src)[6]; \
	(dst)[7] ^= (src)[7]; \
	(dst)[8] ^= (src)[8]; \
	(dst)[9] ^= (src)[9]; \
	(dst)[10] ^= (src)[10]; \
	(dst)[11] ^= (src)[11]; \
	(dst)[12] ^= (src)[12]; \
	(dst)[13] ^= (src)[13]; \
	(dst)[14] ^= (src)[14]; \
	(dst)[15] ^= (src)[15]


#define	AES_MINBITS		128
#define	AES_MAXBITS		256


#define	AES_32BIT_KS		32
#define	AES_64BIT_KS		64

#define	MAX_AES_NR		14 
#define	MAX_AES_NB		4  

typedef union {
#ifdef	sun4u
	uint64_t	ks64[((MAX_AES_NR) + 1) * (MAX_AES_NB)];
#endif
	uint32_t	ks32[((MAX_AES_NR) + 1) * (MAX_AES_NB)];
} aes_ks_t;

typedef struct aes_impl_ops aes_impl_ops_t;


typedef struct aes_key aes_key_t;
struct aes_key {
	aes_ks_t	encr_ks;  
	aes_ks_t	decr_ks;  
#ifdef __amd64
	long double	align128; 
#endif	
	const aes_impl_ops_t	*ops;	
	int		nr;	  
	int		type;	  
};


extern void *aes_alloc_keysched(size_t *size, int kmflag);
extern void aes_init_keysched(const uint8_t *cipherKey, uint_t keyBits,
	void *keysched);
extern int aes_encrypt_block(const void *ks, const uint8_t *pt, uint8_t *ct);
extern int aes_decrypt_block(const void *ks, const uint8_t *ct, uint8_t *pt);


extern void aes_copy_block(uint8_t *in, uint8_t *out);
extern void aes_xor_block(uint8_t *data, uint8_t *dst);


extern int aes_encrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out);
extern int aes_decrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out);


#ifdef _AES_IMPL

typedef enum aes_mech_type {
	AES_ECB_MECH_INFO_TYPE,		
	AES_CBC_MECH_INFO_TYPE,		
	AES_CBC_PAD_MECH_INFO_TYPE,	
	AES_CTR_MECH_INFO_TYPE,		
	AES_CCM_MECH_INFO_TYPE,		
	AES_GCM_MECH_INFO_TYPE,		
	AES_GMAC_MECH_INFO_TYPE		
} aes_mech_type_t;

#endif 


typedef void 		(*aes_generate_f)(aes_key_t *, const uint32_t *, int);
typedef void		(*aes_encrypt_f)(const uint32_t[], int,
    const uint32_t[4], uint32_t[4]);
typedef void		(*aes_decrypt_f)(const uint32_t[], int,
    const uint32_t[4], uint32_t[4]);
typedef boolean_t	(*aes_will_work_f)(void);

#define	AES_IMPL_NAME_MAX (16)

struct aes_impl_ops {
	aes_generate_f generate;
	aes_encrypt_f encrypt;
	aes_decrypt_f decrypt;
	aes_will_work_f is_supported;
	boolean_t needs_byteswap;
	char name[AES_IMPL_NAME_MAX];
};

extern const aes_impl_ops_t aes_generic_impl;
#if defined(__x86_64)
extern const aes_impl_ops_t aes_x86_64_impl;


extern ASMABI int rijndael_key_setup_enc_amd64(uint32_t rk[],
	const uint32_t cipherKey[], int keyBits);
extern ASMABI int rijndael_key_setup_dec_amd64(uint32_t rk[],
	const uint32_t cipherKey[], int keyBits);
extern ASMABI void aes_encrypt_amd64(const uint32_t rk[], int Nr,
	const uint32_t pt[4], uint32_t ct[4]);
extern ASMABI void aes_decrypt_amd64(const uint32_t rk[], int Nr,
	const uint32_t ct[4], uint32_t pt[4]);
#endif
#if defined(__x86_64) && defined(HAVE_AES)
extern const aes_impl_ops_t aes_aesni_impl;
#endif


void aes_impl_init(void);


const struct aes_impl_ops *aes_impl_get_ops(void);

#ifdef	__cplusplus
}
#endif

#endif	
