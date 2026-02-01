
 
#include <linux/crypto.h>
#include "internal.h"

int crypto_comp_compress(struct crypto_comp *comp,
			 const u8 *src, unsigned int slen,
			 u8 *dst, unsigned int *dlen)
{
	struct crypto_tfm *tfm = crypto_comp_tfm(comp);

	return tfm->__crt_alg->cra_compress.coa_compress(tfm, src, slen, dst,
	                                                 dlen);
}
EXPORT_SYMBOL_GPL(crypto_comp_compress);

int crypto_comp_decompress(struct crypto_comp *comp,
			   const u8 *src, unsigned int slen,
			   u8 *dst, unsigned int *dlen)
{
	struct crypto_tfm *tfm = crypto_comp_tfm(comp);

	return tfm->__crt_alg->cra_compress.coa_decompress(tfm, src, slen, dst,
	                                                   dlen);
}
EXPORT_SYMBOL_GPL(crypto_comp_decompress);
