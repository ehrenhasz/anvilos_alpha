 
 

  
 
 

#ifndef UMAC_OUTPUT_LEN
#define UMAC_OUTPUT_LEN     8   
#endif

#if UMAC_OUTPUT_LEN != 4 && UMAC_OUTPUT_LEN != 8 && \
    UMAC_OUTPUT_LEN != 12 && UMAC_OUTPUT_LEN != 16
# error UMAC_OUTPUT_LEN must be defined to 4, 8, 12 or 16
#endif

 
 
 
 
 

 
 
 

#include "includes.h"
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "xmalloc.h"
#include "umac.h"
#include "misc.h"

 
 
 

 
typedef u_int8_t	UINT8;   
typedef u_int16_t	UINT16;  
typedef u_int32_t	UINT32;  
typedef u_int64_t	UINT64;  
typedef unsigned int	UWORD;   

 
 
 

#define UMAC_KEY_LEN           16   

 
 
 

#if BYTE_ORDER == LITTLE_ENDIAN
#define __LITTLE_ENDIAN__ 1
#else
#define __LITTLE_ENDIAN__ 0
#endif

 
 
 
 
 


 
 
 
 
 


 
 
 

#define MUL64(a,b) ((UINT64)((UINT64)(UINT32)(a) * (UINT64)(UINT32)(b)))

 
 
 

#if (__LITTLE_ENDIAN__)
#define LOAD_UINT32_REVERSED(p)		get_u32(p)
#define STORE_UINT32_REVERSED(p,v)	put_u32(p,v)
#else
#define LOAD_UINT32_REVERSED(p)		get_u32_le(p)
#define STORE_UINT32_REVERSED(p,v)	put_u32_le(p,v)
#endif

#define LOAD_UINT32_LITTLE(p)		(get_u32_le(p))
#define STORE_UINT32_BIG(p,v)		put_u32(p, v)

 
 
 
 
 

 
#define AES_BLOCK_LEN  16

 
#ifdef WITH_OPENSSL
#include "openbsd-compat/openssl-compat.h"
#ifndef USE_BUILTIN_RIJNDAEL
# include <openssl/aes.h>
#endif
typedef AES_KEY aes_int_key[1];
#define aes_encryption(in,out,int_key)                  \
  AES_encrypt((u_char *)(in),(u_char *)(out),(AES_KEY *)int_key)
#define aes_key_setup(key,int_key)                      \
  AES_set_encrypt_key((const u_char *)(key),UMAC_KEY_LEN*8,int_key)
#else
#include "rijndael.h"
#define AES_ROUNDS ((UMAC_KEY_LEN / 4) + 6)
typedef UINT8 aes_int_key[AES_ROUNDS+1][4][4];	 
#define aes_encryption(in,out,int_key) \
  rijndaelEncrypt((u32 *)(int_key), AES_ROUNDS, (u8 *)(in), (u8 *)(out))
#define aes_key_setup(key,int_key) \
  rijndaelKeySetupEnc((u32 *)(int_key), (const unsigned char *)(key), \
  UMAC_KEY_LEN*8)
#endif

 
static void kdf(void *bufp, aes_int_key key, UINT8 ndx, int nbytes)
{
    UINT8 in_buf[AES_BLOCK_LEN] = {0};
    UINT8 out_buf[AES_BLOCK_LEN];
    UINT8 *dst_buf = (UINT8 *)bufp;
    int i;

     
    in_buf[AES_BLOCK_LEN-9] = ndx;
    in_buf[AES_BLOCK_LEN-1] = i = 1;

    while (nbytes >= AES_BLOCK_LEN) {
        aes_encryption(in_buf, out_buf, key);
        memcpy(dst_buf,out_buf,AES_BLOCK_LEN);
        in_buf[AES_BLOCK_LEN-1] = ++i;
        nbytes -= AES_BLOCK_LEN;
        dst_buf += AES_BLOCK_LEN;
    }
    if (nbytes) {
        aes_encryption(in_buf, out_buf, key);
        memcpy(dst_buf,out_buf,nbytes);
    }
    explicit_bzero(in_buf, sizeof(in_buf));
    explicit_bzero(out_buf, sizeof(out_buf));
}

 

typedef struct {
    UINT8 cache[AES_BLOCK_LEN];   
    UINT8 nonce[AES_BLOCK_LEN];   
    aes_int_key prf_key;          
} pdf_ctx;

static void pdf_init(pdf_ctx *pc, aes_int_key prf_key)
{
    UINT8 buf[UMAC_KEY_LEN];

    kdf(buf, prf_key, 0, UMAC_KEY_LEN);
    aes_key_setup(buf, pc->prf_key);

     
    memset(pc->nonce, 0, sizeof(pc->nonce));
    aes_encryption(pc->nonce, pc->cache, pc->prf_key);
    explicit_bzero(buf, sizeof(buf));
}

static void pdf_gen_xor(pdf_ctx *pc, const UINT8 nonce[8],
    UINT8 buf[UMAC_OUTPUT_LEN])
{
     

#if (UMAC_OUTPUT_LEN == 4)
#define LOW_BIT_MASK 3
#elif (UMAC_OUTPUT_LEN == 8)
#define LOW_BIT_MASK 1
#elif (UMAC_OUTPUT_LEN > 8)
#define LOW_BIT_MASK 0
#endif
    union {
        UINT8 tmp_nonce_lo[4];
        UINT32 align;
    } t;
#if LOW_BIT_MASK != 0
    int ndx = nonce[7] & LOW_BIT_MASK;
#endif
    *(UINT32 *)t.tmp_nonce_lo = ((const UINT32 *)nonce)[1];
    t.tmp_nonce_lo[3] &= ~LOW_BIT_MASK;  

    if ( (((UINT32 *)t.tmp_nonce_lo)[0] != ((UINT32 *)pc->nonce)[1]) ||
         (((const UINT32 *)nonce)[0] != ((UINT32 *)pc->nonce)[0]) )
    {
        ((UINT32 *)pc->nonce)[0] = ((const UINT32 *)nonce)[0];
        ((UINT32 *)pc->nonce)[1] = ((UINT32 *)t.tmp_nonce_lo)[0];
        aes_encryption(pc->nonce, pc->cache, pc->prf_key);
    }

#if (UMAC_OUTPUT_LEN == 4)
    *((UINT32 *)buf) ^= ((UINT32 *)pc->cache)[ndx];
#elif (UMAC_OUTPUT_LEN == 8)
    *((UINT64 *)buf) ^= ((UINT64 *)pc->cache)[ndx];
#elif (UMAC_OUTPUT_LEN == 12)
    ((UINT64 *)buf)[0] ^= ((UINT64 *)pc->cache)[0];
    ((UINT32 *)buf)[2] ^= ((UINT32 *)pc->cache)[2];
#elif (UMAC_OUTPUT_LEN == 16)
    ((UINT64 *)buf)[0] ^= ((UINT64 *)pc->cache)[0];
    ((UINT64 *)buf)[1] ^= ((UINT64 *)pc->cache)[1];
#endif
}

 
 
 
 
 

 

  

#define STREAMS (UMAC_OUTPUT_LEN / 4)  
#define L1_KEY_LEN         1024      
#define L1_KEY_SHIFT         16      
#define L1_PAD_BOUNDARY      32      
#define ALLOC_BOUNDARY       16      
#define HASH_BUF_BYTES       64      

typedef struct {
    UINT8  nh_key [L1_KEY_LEN + L1_KEY_SHIFT * (STREAMS - 1)];  
    UINT8  data   [HASH_BUF_BYTES];     
    int next_data_empty;     
    int bytes_hashed;        
    UINT64 state[STREAMS];                
} nh_ctx;


#if (UMAC_OUTPUT_LEN == 4)

static void nh_aux(void *kp, const void *dp, void *hp, UINT32 dlen)
 
{
    UINT64 h;
    UWORD c = dlen / 32;
    UINT32 *k = (UINT32 *)kp;
    const UINT32 *d = (const UINT32 *)dp;
    UINT32 d0,d1,d2,d3,d4,d5,d6,d7;
    UINT32 k0,k1,k2,k3,k4,k5,k6,k7;

    h = *((UINT64 *)hp);
    do {
        d0 = LOAD_UINT32_LITTLE(d+0); d1 = LOAD_UINT32_LITTLE(d+1);
        d2 = LOAD_UINT32_LITTLE(d+2); d3 = LOAD_UINT32_LITTLE(d+3);
        d4 = LOAD_UINT32_LITTLE(d+4); d5 = LOAD_UINT32_LITTLE(d+5);
        d6 = LOAD_UINT32_LITTLE(d+6); d7 = LOAD_UINT32_LITTLE(d+7);
        k0 = *(k+0); k1 = *(k+1); k2 = *(k+2); k3 = *(k+3);
        k4 = *(k+4); k5 = *(k+5); k6 = *(k+6); k7 = *(k+7);
        h += MUL64((k0 + d0), (k4 + d4));
        h += MUL64((k1 + d1), (k5 + d5));
        h += MUL64((k2 + d2), (k6 + d6));
        h += MUL64((k3 + d3), (k7 + d7));

        d += 8;
        k += 8;
    } while (--c);
  *((UINT64 *)hp) = h;
}

#elif (UMAC_OUTPUT_LEN == 8)

static void nh_aux(void *kp, const void *dp, void *hp, UINT32 dlen)
 
{
  UINT64 h1,h2;
  UWORD c = dlen / 32;
  UINT32 *k = (UINT32 *)kp;
  const UINT32 *d = (const UINT32 *)dp;
  UINT32 d0,d1,d2,d3,d4,d5,d6,d7;
  UINT32 k0,k1,k2,k3,k4,k5,k6,k7,
        k8,k9,k10,k11;

  h1 = *((UINT64 *)hp);
  h2 = *((UINT64 *)hp + 1);
  k0 = *(k+0); k1 = *(k+1); k2 = *(k+2); k3 = *(k+3);
  do {
    d0 = LOAD_UINT32_LITTLE(d+0); d1 = LOAD_UINT32_LITTLE(d+1);
    d2 = LOAD_UINT32_LITTLE(d+2); d3 = LOAD_UINT32_LITTLE(d+3);
    d4 = LOAD_UINT32_LITTLE(d+4); d5 = LOAD_UINT32_LITTLE(d+5);
    d6 = LOAD_UINT32_LITTLE(d+6); d7 = LOAD_UINT32_LITTLE(d+7);
    k4 = *(k+4); k5 = *(k+5); k6 = *(k+6); k7 = *(k+7);
    k8 = *(k+8); k9 = *(k+9); k10 = *(k+10); k11 = *(k+11);

    h1 += MUL64((k0 + d0), (k4 + d4));
    h2 += MUL64((k4 + d0), (k8 + d4));

    h1 += MUL64((k1 + d1), (k5 + d5));
    h2 += MUL64((k5 + d1), (k9 + d5));

    h1 += MUL64((k2 + d2), (k6 + d6));
    h2 += MUL64((k6 + d2), (k10 + d6));

    h1 += MUL64((k3 + d3), (k7 + d7));
    h2 += MUL64((k7 + d3), (k11 + d7));

    k0 = k8; k1 = k9; k2 = k10; k3 = k11;

    d += 8;
    k += 8;
  } while (--c);
  ((UINT64 *)hp)[0] = h1;
  ((UINT64 *)hp)[1] = h2;
}

#elif (UMAC_OUTPUT_LEN == 12)

static void nh_aux(void *kp, const void *dp, void *hp, UINT32 dlen)
 
{
    UINT64 h1,h2,h3;
    UWORD c = dlen / 32;
    UINT32 *k = (UINT32 *)kp;
    const UINT32 *d = (const UINT32 *)dp;
    UINT32 d0,d1,d2,d3,d4,d5,d6,d7;
    UINT32 k0,k1,k2,k3,k4,k5,k6,k7,
        k8,k9,k10,k11,k12,k13,k14,k15;

    h1 = *((UINT64 *)hp);
    h2 = *((UINT64 *)hp + 1);
    h3 = *((UINT64 *)hp + 2);
    k0 = *(k+0); k1 = *(k+1); k2 = *(k+2); k3 = *(k+3);
    k4 = *(k+4); k5 = *(k+5); k6 = *(k+6); k7 = *(k+7);
    do {
        d0 = LOAD_UINT32_LITTLE(d+0); d1 = LOAD_UINT32_LITTLE(d+1);
        d2 = LOAD_UINT32_LITTLE(d+2); d3 = LOAD_UINT32_LITTLE(d+3);
        d4 = LOAD_UINT32_LITTLE(d+4); d5 = LOAD_UINT32_LITTLE(d+5);
        d6 = LOAD_UINT32_LITTLE(d+6); d7 = LOAD_UINT32_LITTLE(d+7);
        k8 = *(k+8); k9 = *(k+9); k10 = *(k+10); k11 = *(k+11);
        k12 = *(k+12); k13 = *(k+13); k14 = *(k+14); k15 = *(k+15);

        h1 += MUL64((k0 + d0), (k4 + d4));
        h2 += MUL64((k4 + d0), (k8 + d4));
        h3 += MUL64((k8 + d0), (k12 + d4));

        h1 += MUL64((k1 + d1), (k5 + d5));
        h2 += MUL64((k5 + d1), (k9 + d5));
        h3 += MUL64((k9 + d1), (k13 + d5));

        h1 += MUL64((k2 + d2), (k6 + d6));
        h2 += MUL64((k6 + d2), (k10 + d6));
        h3 += MUL64((k10 + d2), (k14 + d6));

        h1 += MUL64((k3 + d3), (k7 + d7));
        h2 += MUL64((k7 + d3), (k11 + d7));
        h3 += MUL64((k11 + d3), (k15 + d7));

        k0 = k8; k1 = k9; k2 = k10; k3 = k11;
        k4 = k12; k5 = k13; k6 = k14; k7 = k15;

        d += 8;
        k += 8;
    } while (--c);
    ((UINT64 *)hp)[0] = h1;
    ((UINT64 *)hp)[1] = h2;
    ((UINT64 *)hp)[2] = h3;
}

#elif (UMAC_OUTPUT_LEN == 16)

static void nh_aux(void *kp, const void *dp, void *hp, UINT32 dlen)
 
{
    UINT64 h1,h2,h3,h4;
    UWORD c = dlen / 32;
    UINT32 *k = (UINT32 *)kp;
    const UINT32 *d = (const UINT32 *)dp;
    UINT32 d0,d1,d2,d3,d4,d5,d6,d7;
    UINT32 k0,k1,k2,k3,k4,k5,k6,k7,
        k8,k9,k10,k11,k12,k13,k14,k15,
        k16,k17,k18,k19;

    h1 = *((UINT64 *)hp);
    h2 = *((UINT64 *)hp + 1);
    h3 = *((UINT64 *)hp + 2);
    h4 = *((UINT64 *)hp + 3);
    k0 = *(k+0); k1 = *(k+1); k2 = *(k+2); k3 = *(k+3);
    k4 = *(k+4); k5 = *(k+5); k6 = *(k+6); k7 = *(k+7);
    do {
        d0 = LOAD_UINT32_LITTLE(d+0); d1 = LOAD_UINT32_LITTLE(d+1);
        d2 = LOAD_UINT32_LITTLE(d+2); d3 = LOAD_UINT32_LITTLE(d+3);
        d4 = LOAD_UINT32_LITTLE(d+4); d5 = LOAD_UINT32_LITTLE(d+5);
        d6 = LOAD_UINT32_LITTLE(d+6); d7 = LOAD_UINT32_LITTLE(d+7);
        k8 = *(k+8); k9 = *(k+9); k10 = *(k+10); k11 = *(k+11);
        k12 = *(k+12); k13 = *(k+13); k14 = *(k+14); k15 = *(k+15);
        k16 = *(k+16); k17 = *(k+17); k18 = *(k+18); k19 = *(k+19);

        h1 += MUL64((k0 + d0), (k4 + d4));
        h2 += MUL64((k4 + d0), (k8 + d4));
        h3 += MUL64((k8 + d0), (k12 + d4));
        h4 += MUL64((k12 + d0), (k16 + d4));

        h1 += MUL64((k1 + d1), (k5 + d5));
        h2 += MUL64((k5 + d1), (k9 + d5));
        h3 += MUL64((k9 + d1), (k13 + d5));
        h4 += MUL64((k13 + d1), (k17 + d5));

        h1 += MUL64((k2 + d2), (k6 + d6));
        h2 += MUL64((k6 + d2), (k10 + d6));
        h3 += MUL64((k10 + d2), (k14 + d6));
        h4 += MUL64((k14 + d2), (k18 + d6));

        h1 += MUL64((k3 + d3), (k7 + d7));
        h2 += MUL64((k7 + d3), (k11 + d7));
        h3 += MUL64((k11 + d3), (k15 + d7));
        h4 += MUL64((k15 + d3), (k19 + d7));

        k0 = k8; k1 = k9; k2 = k10; k3 = k11;
        k4 = k12; k5 = k13; k6 = k14; k7 = k15;
        k8 = k16; k9 = k17; k10 = k18; k11 = k19;

        d += 8;
        k += 8;
    } while (--c);
    ((UINT64 *)hp)[0] = h1;
    ((UINT64 *)hp)[1] = h2;
    ((UINT64 *)hp)[2] = h3;
    ((UINT64 *)hp)[3] = h4;
}

 
#endif   
 


 

static void nh_transform(nh_ctx *hc, const UINT8 *buf, UINT32 nbytes)
 
{
    UINT8 *key;

    key = hc->nh_key + hc->bytes_hashed;
    nh_aux(key, buf, hc->state, nbytes);
}

 

#if (__LITTLE_ENDIAN__)
static void endian_convert(void *buf, UWORD bpw, UINT32 num_bytes)
 
 
{
    UWORD iters = num_bytes / bpw;
    if (bpw == 4) {
        UINT32 *p = (UINT32 *)buf;
        do {
            *p = LOAD_UINT32_REVERSED(p);
            p++;
        } while (--iters);
    } else if (bpw == 8) {
        UINT32 *p = (UINT32 *)buf;
        UINT32 t;
        do {
            t = LOAD_UINT32_REVERSED(p+1);
            p[1] = LOAD_UINT32_REVERSED(p);
            p[0] = t;
            p += 2;
        } while (--iters);
    }
}
#define endian_convert_if_le(x,y,z) endian_convert((x),(y),(z))
#else
#define endian_convert_if_le(x,y,z) do{}while(0)   
#endif

 

static void nh_reset(nh_ctx *hc)
 
{
    hc->bytes_hashed = 0;
    hc->next_data_empty = 0;
    hc->state[0] = 0;
#if (UMAC_OUTPUT_LEN >= 8)
    hc->state[1] = 0;
#endif
#if (UMAC_OUTPUT_LEN >= 12)
    hc->state[2] = 0;
#endif
#if (UMAC_OUTPUT_LEN == 16)
    hc->state[3] = 0;
#endif

}

 

static void nh_init(nh_ctx *hc, aes_int_key prf_key)
 
{
    kdf(hc->nh_key, prf_key, 1, sizeof(hc->nh_key));
    endian_convert_if_le(hc->nh_key, 4, sizeof(hc->nh_key));
    nh_reset(hc);
}

 

static void nh_update(nh_ctx *hc, const UINT8 *buf, UINT32 nbytes)
 
 
{
    UINT32 i,j;

    j = hc->next_data_empty;
    if ((j + nbytes) >= HASH_BUF_BYTES) {
        if (j) {
            i = HASH_BUF_BYTES - j;
            memcpy(hc->data+j, buf, i);
            nh_transform(hc,hc->data,HASH_BUF_BYTES);
            nbytes -= i;
            buf += i;
            hc->bytes_hashed += HASH_BUF_BYTES;
        }
        if (nbytes >= HASH_BUF_BYTES) {
            i = nbytes & ~(HASH_BUF_BYTES - 1);
            nh_transform(hc, buf, i);
            nbytes -= i;
            buf += i;
            hc->bytes_hashed += i;
        }
        j = 0;
    }
    memcpy(hc->data + j, buf, nbytes);
    hc->next_data_empty = j + nbytes;
}

 

static void zero_pad(UINT8 *p, int nbytes)
{
 
    if (nbytes >= (int)sizeof(UWORD)) {
        while ((ptrdiff_t)p % sizeof(UWORD)) {
            *p = 0;
            nbytes--;
            p++;
        }
        while (nbytes >= (int)sizeof(UWORD)) {
            *(UWORD *)p = 0;
            nbytes -= sizeof(UWORD);
            p += sizeof(UWORD);
        }
    }
    while (nbytes) {
        *p = 0;
        nbytes--;
        p++;
    }
}

 

static void nh_final(nh_ctx *hc, UINT8 *result)
 
{
    int nh_len, nbits;

    if (hc->next_data_empty != 0) {
        nh_len = ((hc->next_data_empty + (L1_PAD_BOUNDARY - 1)) &
                                                ~(L1_PAD_BOUNDARY - 1));
        zero_pad(hc->data + hc->next_data_empty,
                                          nh_len - hc->next_data_empty);
        nh_transform(hc, hc->data, nh_len);
        hc->bytes_hashed += hc->next_data_empty;
    } else if (hc->bytes_hashed == 0) {
	nh_len = L1_PAD_BOUNDARY;
        zero_pad(hc->data, L1_PAD_BOUNDARY);
        nh_transform(hc, hc->data, nh_len);
    }

    nbits = (hc->bytes_hashed << 3);
    ((UINT64 *)result)[0] = ((UINT64 *)hc->state)[0] + nbits;
#if (UMAC_OUTPUT_LEN >= 8)
    ((UINT64 *)result)[1] = ((UINT64 *)hc->state)[1] + nbits;
#endif
#if (UMAC_OUTPUT_LEN >= 12)
    ((UINT64 *)result)[2] = ((UINT64 *)hc->state)[2] + nbits;
#endif
#if (UMAC_OUTPUT_LEN == 16)
    ((UINT64 *)result)[3] = ((UINT64 *)hc->state)[3] + nbits;
#endif
    nh_reset(hc);
}

 

static void nh(nh_ctx *hc, const UINT8 *buf, UINT32 padded_len,
               UINT32 unpadded_len, UINT8 *result)
 
{
    UINT32 nbits;

     
    nbits = (unpadded_len << 3);

    ((UINT64 *)result)[0] = nbits;
#if (UMAC_OUTPUT_LEN >= 8)
    ((UINT64 *)result)[1] = nbits;
#endif
#if (UMAC_OUTPUT_LEN >= 12)
    ((UINT64 *)result)[2] = nbits;
#endif
#if (UMAC_OUTPUT_LEN == 16)
    ((UINT64 *)result)[3] = nbits;
#endif

    nh_aux(hc->nh_key, buf, result, padded_len);
}

 
 
 
 
 

 

 
 
 

 
 
 

 
#define p36    ((UINT64)0x0000000FFFFFFFFBull)               
#define p64    ((UINT64)0xFFFFFFFFFFFFFFC5ull)               
#define m36    ((UINT64)0x0000000FFFFFFFFFull)   


 

typedef struct uhash_ctx {
    nh_ctx hash;                           
    UINT64 poly_key_8[STREAMS];            
    UINT64 poly_accum[STREAMS];            
    UINT64 ip_keys[STREAMS*4];             
    UINT32 ip_trans[STREAMS];              
    UINT32 msg_len;                        
                                           
} uhash_ctx;
typedef struct uhash_ctx *uhash_ctx_t;

 


 

static UINT64 poly64(UINT64 cur, UINT64 key, UINT64 data)
{
    UINT32 key_hi = (UINT32)(key >> 32),
           key_lo = (UINT32)key,
           cur_hi = (UINT32)(cur >> 32),
           cur_lo = (UINT32)cur,
           x_lo,
           x_hi;
    UINT64 X,T,res;

    X =  MUL64(key_hi, cur_lo) + MUL64(cur_hi, key_lo);
    x_lo = (UINT32)X;
    x_hi = (UINT32)(X >> 32);

    res = (MUL64(key_hi, cur_hi) + x_hi) * 59 + MUL64(key_lo, cur_lo);

    T = ((UINT64)x_lo << 32);
    res += T;
    if (res < T)
        res += 59;

    res += data;
    if (res < data)
        res += 59;

    return res;
}


 
static void poly_hash(uhash_ctx_t hc, UINT32 data_in[])
{
    int i;
    UINT64 *data=(UINT64*)data_in;

    for (i = 0; i < STREAMS; i++) {
        if ((UINT32)(data[i] >> 32) == 0xfffffffful) {
            hc->poly_accum[i] = poly64(hc->poly_accum[i],
                                       hc->poly_key_8[i], p64 - 1);
            hc->poly_accum[i] = poly64(hc->poly_accum[i],
                                       hc->poly_key_8[i], (data[i] - 59));
        } else {
            hc->poly_accum[i] = poly64(hc->poly_accum[i],
                                       hc->poly_key_8[i], data[i]);
        }
    }
}


 


 

static UINT64 ip_aux(UINT64 t, UINT64 *ipkp, UINT64 data)
{
    t = t + ipkp[0] * (UINT64)(UINT16)(data >> 48);
    t = t + ipkp[1] * (UINT64)(UINT16)(data >> 32);
    t = t + ipkp[2] * (UINT64)(UINT16)(data >> 16);
    t = t + ipkp[3] * (UINT64)(UINT16)(data);

    return t;
}

static UINT32 ip_reduce_p36(UINT64 t)
{
 
    UINT64 ret;

    ret = (t & m36) + 5 * (t >> 36);
    if (ret >= p36)
        ret -= p36;

     
    return (UINT32)(ret);
}


 
static void ip_short(uhash_ctx_t ahc, UINT8 *nh_res, u_char *res)
{
    UINT64 t;
    UINT64 *nhp = (UINT64 *)nh_res;

    t  = ip_aux(0,ahc->ip_keys, nhp[0]);
    STORE_UINT32_BIG((UINT32 *)res+0, ip_reduce_p36(t) ^ ahc->ip_trans[0]);
#if (UMAC_OUTPUT_LEN >= 8)
    t  = ip_aux(0,ahc->ip_keys+4, nhp[1]);
    STORE_UINT32_BIG((UINT32 *)res+1, ip_reduce_p36(t) ^ ahc->ip_trans[1]);
#endif
#if (UMAC_OUTPUT_LEN >= 12)
    t  = ip_aux(0,ahc->ip_keys+8, nhp[2]);
    STORE_UINT32_BIG((UINT32 *)res+2, ip_reduce_p36(t) ^ ahc->ip_trans[2]);
#endif
#if (UMAC_OUTPUT_LEN == 16)
    t  = ip_aux(0,ahc->ip_keys+12, nhp[3]);
    STORE_UINT32_BIG((UINT32 *)res+3, ip_reduce_p36(t) ^ ahc->ip_trans[3]);
#endif
}

 
static void ip_long(uhash_ctx_t ahc, u_char *res)
{
    int i;
    UINT64 t;

    for (i = 0; i < STREAMS; i++) {
         
        if (ahc->poly_accum[i] >= p64)
            ahc->poly_accum[i] -= p64;
        t  = ip_aux(0,ahc->ip_keys+(i*4), ahc->poly_accum[i]);
        STORE_UINT32_BIG((UINT32 *)res+i,
                         ip_reduce_p36(t) ^ ahc->ip_trans[i]);
    }
}


 

 

 
static int uhash_reset(uhash_ctx_t pc)
{
    nh_reset(&pc->hash);
    pc->msg_len = 0;
    pc->poly_accum[0] = 1;
#if (UMAC_OUTPUT_LEN >= 8)
    pc->poly_accum[1] = 1;
#endif
#if (UMAC_OUTPUT_LEN >= 12)
    pc->poly_accum[2] = 1;
#endif
#if (UMAC_OUTPUT_LEN == 16)
    pc->poly_accum[3] = 1;
#endif
    return 1;
}

 

 
static void uhash_init(uhash_ctx_t ahc, aes_int_key prf_key)
{
    int i;
    UINT8 buf[(8*STREAMS+4)*sizeof(UINT64)];

     
    memset(ahc, 0, sizeof(uhash_ctx));

     
    nh_init(&ahc->hash, prf_key);

     
    kdf(buf, prf_key, 2, sizeof(buf));     
    for (i = 0; i < STREAMS; i++) {
         
        memcpy(ahc->poly_key_8+i, buf+24*i, 8);
        endian_convert_if_le(ahc->poly_key_8+i, 8, 8);
         
        ahc->poly_key_8[i] &= ((UINT64)0x01ffffffu << 32) + 0x01ffffffu;
        ahc->poly_accum[i] = 1;   
    }

     
    kdf(buf, prf_key, 3, sizeof(buf));  
    for (i = 0; i < STREAMS; i++)
          memcpy(ahc->ip_keys+4*i, buf+(8*i+4)*sizeof(UINT64),
                                                 4*sizeof(UINT64));
    endian_convert_if_le(ahc->ip_keys, sizeof(UINT64),
                                                  sizeof(ahc->ip_keys));
    for (i = 0; i < STREAMS*4; i++)
        ahc->ip_keys[i] %= p36;   

     
     
    kdf(ahc->ip_trans, prf_key, 4, STREAMS * sizeof(UINT32));
    endian_convert_if_le(ahc->ip_trans, sizeof(UINT32),
                         STREAMS * sizeof(UINT32));
    explicit_bzero(buf, sizeof(buf));
}

 

#if 0
static uhash_ctx_t uhash_alloc(u_char key[])
{
 
    uhash_ctx_t ctx;
    u_char bytes_to_add;
    aes_int_key prf_key;

    ctx = (uhash_ctx_t)malloc(sizeof(uhash_ctx)+ALLOC_BOUNDARY);
    if (ctx) {
        if (ALLOC_BOUNDARY) {
            bytes_to_add = ALLOC_BOUNDARY -
                              ((ptrdiff_t)ctx & (ALLOC_BOUNDARY -1));
            ctx = (uhash_ctx_t)((u_char *)ctx + bytes_to_add);
            *((u_char *)ctx - 1) = bytes_to_add;
        }
        aes_key_setup(key,prf_key);
        uhash_init(ctx, prf_key);
    }
    return (ctx);
}
#endif

 

#if 0
static int uhash_free(uhash_ctx_t ctx)
{
 
    u_char bytes_to_sub;

    if (ctx) {
        if (ALLOC_BOUNDARY) {
            bytes_to_sub = *((u_char *)ctx - 1);
            ctx = (uhash_ctx_t)((u_char *)ctx - bytes_to_sub);
        }
        free(ctx);
    }
    return (1);
}
#endif
 

static int uhash_update(uhash_ctx_t ctx, const u_char *input, long len)
 
{
    UWORD bytes_hashed, bytes_remaining;
    UINT64 result_buf[STREAMS];
    UINT8 *nh_result = (UINT8 *)&result_buf;

    if (ctx->msg_len + len <= L1_KEY_LEN) {
        nh_update(&ctx->hash, (const UINT8 *)input, len);
        ctx->msg_len += len;
    } else {

         bytes_hashed = ctx->msg_len % L1_KEY_LEN;
         if (ctx->msg_len == L1_KEY_LEN)
             bytes_hashed = L1_KEY_LEN;

         if (bytes_hashed + len >= L1_KEY_LEN) {

              
              
              
             if (bytes_hashed) {
                 bytes_remaining = (L1_KEY_LEN - bytes_hashed);
                 nh_update(&ctx->hash, (const UINT8 *)input, bytes_remaining);
                 nh_final(&ctx->hash, nh_result);
                 ctx->msg_len += bytes_remaining;
                 poly_hash(ctx,(UINT32 *)nh_result);
                 len -= bytes_remaining;
                 input += bytes_remaining;
             }

              
             while (len >= L1_KEY_LEN) {
                 nh(&ctx->hash, (const UINT8 *)input, L1_KEY_LEN,
                                   L1_KEY_LEN, nh_result);
                 ctx->msg_len += L1_KEY_LEN;
                 len -= L1_KEY_LEN;
                 input += L1_KEY_LEN;
                 poly_hash(ctx,(UINT32 *)nh_result);
             }
         }

          
         if (len) {
             nh_update(&ctx->hash, (const UINT8 *)input, len);
             ctx->msg_len += len;
         }
     }

    return (1);
}

 

static int uhash_final(uhash_ctx_t ctx, u_char *res)
 
{
    UINT64 result_buf[STREAMS];
    UINT8 *nh_result = (UINT8 *)&result_buf;

    if (ctx->msg_len > L1_KEY_LEN) {
        if (ctx->msg_len % L1_KEY_LEN) {
            nh_final(&ctx->hash, nh_result);
            poly_hash(ctx,(UINT32 *)nh_result);
        }
        ip_long(ctx, res);
    } else {
        nh_final(&ctx->hash, nh_result);
        ip_short(ctx,nh_result, res);
    }
    uhash_reset(ctx);
    return (1);
}

 

#if 0
static int uhash(uhash_ctx_t ahc, u_char *msg, long len, u_char *res)
 
 
{
    UINT8 nh_result[STREAMS*sizeof(UINT64)];
    UINT32 nh_len;
    int extra_zeroes_needed;

     
    if (len <= L1_KEY_LEN) {
	if (len == 0)                   
		nh_len = L1_PAD_BOUNDARY;   
	else
		nh_len = ((len + (L1_PAD_BOUNDARY - 1)) & ~(L1_PAD_BOUNDARY - 1));
        extra_zeroes_needed = nh_len - len;
        zero_pad((UINT8 *)msg + len, extra_zeroes_needed);
        nh(&ahc->hash, (UINT8 *)msg, nh_len, len, nh_result);
        ip_short(ahc,nh_result, res);
    } else {
         
        do {
            nh(&ahc->hash, (UINT8 *)msg, L1_KEY_LEN, L1_KEY_LEN, nh_result);
            poly_hash(ahc,(UINT32 *)nh_result);
            len -= L1_KEY_LEN;
            msg += L1_KEY_LEN;
        } while (len >= L1_KEY_LEN);
        if (len) {
            nh_len = ((len + (L1_PAD_BOUNDARY - 1)) & ~(L1_PAD_BOUNDARY - 1));
            extra_zeroes_needed = nh_len - len;
            zero_pad((UINT8 *)msg + len, extra_zeroes_needed);
            nh(&ahc->hash, (UINT8 *)msg, nh_len, len, nh_result);
            poly_hash(ahc,(UINT32 *)nh_result);
        }

        ip_long(ahc, res);
    }

    uhash_reset(ahc);
    return 1;
}
#endif

 
 
 
 
 

 
struct umac_ctx {
    uhash_ctx hash;           
    pdf_ctx pdf;              
    void *free_ptr;           
} umac_ctx;

 

#if 0
int umac_reset(struct umac_ctx *ctx)
 
{
    uhash_reset(&ctx->hash);
    return (1);
}
#endif

 

int umac_delete(struct umac_ctx *ctx)
 
{
    if (ctx) {
        if (ALLOC_BOUNDARY)
            ctx = (struct umac_ctx *)ctx->free_ptr;
        freezero(ctx, sizeof(*ctx) + ALLOC_BOUNDARY);
    }
    return (1);
}

 

struct umac_ctx *umac_new(const u_char key[])
 
{
    struct umac_ctx *ctx, *octx;
    size_t bytes_to_add;
    aes_int_key prf_key;

    octx = ctx = xcalloc(1, sizeof(*ctx) + ALLOC_BOUNDARY);
    if (ctx) {
        if (ALLOC_BOUNDARY) {
            bytes_to_add = ALLOC_BOUNDARY -
                              ((ptrdiff_t)ctx & (ALLOC_BOUNDARY - 1));
            ctx = (struct umac_ctx *)((u_char *)ctx + bytes_to_add);
        }
        ctx->free_ptr = octx;
        aes_key_setup(key, prf_key);
        pdf_init(&ctx->pdf, prf_key);
        uhash_init(&ctx->hash, prf_key);
        explicit_bzero(prf_key, sizeof(prf_key));
    }

    return (ctx);
}

 

int umac_final(struct umac_ctx *ctx, u_char tag[], const u_char nonce[8])
 
{
    uhash_final(&ctx->hash, (u_char *)tag);
    pdf_gen_xor(&ctx->pdf, (const UINT8 *)nonce, (UINT8 *)tag);

    return (1);
}

 

int umac_update(struct umac_ctx *ctx, const u_char *input, long len)
 
 
 
{
    uhash_update(&ctx->hash, input, len);
    return (1);
}

 

#if 0
int umac(struct umac_ctx *ctx, u_char *input,
         long len, u_char tag[],
         u_char nonce[8])
 
{
    uhash(&ctx->hash, input, len, (u_char *)tag);
    pdf_gen_xor(&ctx->pdf, (UINT8 *)nonce, (UINT8 *)tag);

    return (1);
}
#endif

 
 
 
 
 
