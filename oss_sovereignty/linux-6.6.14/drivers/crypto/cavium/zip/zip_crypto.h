#ifndef __ZIP_CRYPTO_H__
#define __ZIP_CRYPTO_H__
#include <linux/crypto.h>
#include <crypto/internal/scompress.h>
#include "common.h"
#include "zip_deflate.h"
#include "zip_inflate.h"
struct zip_kernel_ctx {
	struct zip_operation zip_comp;
	struct zip_operation zip_decomp;
};
int  zip_alloc_comp_ctx_deflate(struct crypto_tfm *tfm);
int  zip_alloc_comp_ctx_lzs(struct crypto_tfm *tfm);
void zip_free_comp_ctx(struct crypto_tfm *tfm);
int  zip_comp_compress(struct crypto_tfm *tfm,
		       const u8 *src, unsigned int slen,
		       u8 *dst, unsigned int *dlen);
int  zip_comp_decompress(struct crypto_tfm *tfm,
			 const u8 *src, unsigned int slen,
			 u8 *dst, unsigned int *dlen);
void *zip_alloc_scomp_ctx_deflate(struct crypto_scomp *tfm);
void *zip_alloc_scomp_ctx_lzs(struct crypto_scomp *tfm);
void  zip_free_scomp_ctx(struct crypto_scomp *tfm, void *zip_ctx);
int   zip_scomp_compress(struct crypto_scomp *tfm,
			 const u8 *src, unsigned int slen,
			 u8 *dst, unsigned int *dlen, void *ctx);
int   zip_scomp_decompress(struct crypto_scomp *tfm,
			   const u8 *src, unsigned int slen,
			   u8 *dst, unsigned int *dlen, void *ctx);
#endif
