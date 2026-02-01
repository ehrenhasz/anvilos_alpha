 

 

#include <crypto/skcipher.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/sunrpc/gss_krb5.h>
#include <linux/sunrpc/xdr.h>
#include <linux/lcm.h>
#include <crypto/hash.h>
#include <kunit/visibility.h>

#include "gss_krb5_internal.h"

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
# define RPCDBG_FACILITY        RPCDBG_AUTH
#endif

 
VISIBLE_IF_KUNIT
void krb5_nfold(u32 inbits, const u8 *in, u32 outbits, u8 *out)
{
	unsigned long ulcm;
	int byte, i, msbit;

	 

	inbits >>= 3;
	outbits >>= 3;

	 
	ulcm = lcm(inbits, outbits);

	 

	memset(out, 0, outbits);
	byte = 0;

	 
	for (i = ulcm-1; i >= 0; i--) {
		 
		msbit = (
			 
			 ((inbits << 3) - 1)
			  
			 + (((inbits << 3) + 13) * (i/inbits))
			  
			 + ((inbits - (i % inbits)) << 3)
			 ) % (inbits << 3);

		 
		byte += (((in[((inbits - 1) - (msbit >> 3)) % inbits] << 8)|
				  (in[((inbits) - (msbit >> 3)) % inbits]))
				 >> ((msbit & 7) + 1)) & 0xff;

		 
		byte += out[i % outbits];
		out[i % outbits] = byte & 0xff;

		 
		byte >>= 8;

	}

	 
	if (byte) {
		for (i = outbits - 1; i >= 0; i--) {
			 
			byte += out[i];
			out[i] = byte & 0xff;

			 
			byte >>= 8;
		}
	}
}
EXPORT_SYMBOL_IF_KUNIT(krb5_nfold);

 
static int krb5_DK(const struct gss_krb5_enctype *gk5e,
		   const struct xdr_netobj *inkey, u8 *rawkey,
		   const struct xdr_netobj *in_constant, gfp_t gfp_mask)
{
	size_t blocksize, keybytes, keylength, n;
	unsigned char *inblockdata, *outblockdata;
	struct xdr_netobj inblock, outblock;
	struct crypto_sync_skcipher *cipher;
	int ret = -EINVAL;

	keybytes = gk5e->keybytes;
	keylength = gk5e->keylength;

	if (inkey->len != keylength)
		goto err_return;

	cipher = crypto_alloc_sync_skcipher(gk5e->encrypt_name, 0, 0);
	if (IS_ERR(cipher))
		goto err_return;
	blocksize = crypto_sync_skcipher_blocksize(cipher);
	if (crypto_sync_skcipher_setkey(cipher, inkey->data, inkey->len))
		goto err_return;

	ret = -ENOMEM;
	inblockdata = kmalloc(blocksize, gfp_mask);
	if (inblockdata == NULL)
		goto err_free_cipher;

	outblockdata = kmalloc(blocksize, gfp_mask);
	if (outblockdata == NULL)
		goto err_free_in;

	inblock.data = (char *) inblockdata;
	inblock.len = blocksize;

	outblock.data = (char *) outblockdata;
	outblock.len = blocksize;

	 

	if (in_constant->len == inblock.len) {
		memcpy(inblock.data, in_constant->data, inblock.len);
	} else {
		krb5_nfold(in_constant->len * 8, in_constant->data,
			   inblock.len * 8, inblock.data);
	}

	 

	n = 0;
	while (n < keybytes) {
		krb5_encrypt(cipher, NULL, inblock.data, outblock.data,
			     inblock.len);

		if ((keybytes - n) <= outblock.len) {
			memcpy(rawkey + n, outblock.data, (keybytes - n));
			break;
		}

		memcpy(rawkey + n, outblock.data, outblock.len);
		memcpy(inblock.data, outblock.data, outblock.len);
		n += outblock.len;
	}

	ret = 0;

	kfree_sensitive(outblockdata);
err_free_in:
	kfree_sensitive(inblockdata);
err_free_cipher:
	crypto_free_sync_skcipher(cipher);
err_return:
	return ret;
}

 
static int krb5_random_to_key_v2(const struct gss_krb5_enctype *gk5e,
				 struct xdr_netobj *randombits,
				 struct xdr_netobj *key)
{
	int ret = -EINVAL;

	if (key->len != 16 && key->len != 32) {
		dprintk("%s: key->len is %d\n", __func__, key->len);
		goto err_out;
	}
	if (randombits->len != 16 && randombits->len != 32) {
		dprintk("%s: randombits->len is %d\n",
			__func__, randombits->len);
		goto err_out;
	}
	if (randombits->len != key->len) {
		dprintk("%s: randombits->len is %d, key->len is %d\n",
			__func__, randombits->len, key->len);
		goto err_out;
	}
	memcpy(key->data, randombits->data, key->len);
	ret = 0;
err_out:
	return ret;
}

 
int krb5_derive_key_v2(const struct gss_krb5_enctype *gk5e,
		       const struct xdr_netobj *inkey,
		       struct xdr_netobj *outkey,
		       const struct xdr_netobj *label,
		       gfp_t gfp_mask)
{
	struct xdr_netobj inblock;
	int ret;

	inblock.len = gk5e->keybytes;
	inblock.data = kmalloc(inblock.len, gfp_mask);
	if (!inblock.data)
		return -ENOMEM;

	ret = krb5_DK(gk5e, inkey, inblock.data, label, gfp_mask);
	if (!ret)
		ret = krb5_random_to_key_v2(gk5e, &inblock, outkey);

	kfree_sensitive(inblock.data);
	return ret;
}

 
static int
krb5_cmac_Ki(struct crypto_shash *tfm, const struct xdr_netobj *constant,
	     u32 outlen, u32 count, struct xdr_netobj *step)
{
	__be32 k = cpu_to_be32(outlen * 8);
	SHASH_DESC_ON_STACK(desc, tfm);
	__be32 i = cpu_to_be32(count);
	u8 zero = 0;
	int ret;

	desc->tfm = tfm;
	ret = crypto_shash_init(desc);
	if (ret)
		goto out_err;

	ret = crypto_shash_update(desc, step->data, step->len);
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, (u8 *)&i, sizeof(i));
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, constant->data, constant->len);
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, &zero, sizeof(zero));
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, (u8 *)&k, sizeof(k));
	if (ret)
		goto out_err;
	ret = crypto_shash_final(desc, step->data);
	if (ret)
		goto out_err;

out_err:
	shash_desc_zero(desc);
	return ret;
}

 
int
krb5_kdf_feedback_cmac(const struct gss_krb5_enctype *gk5e,
		       const struct xdr_netobj *inkey,
		       struct xdr_netobj *outkey,
		       const struct xdr_netobj *constant,
		       gfp_t gfp_mask)
{
	struct xdr_netobj step = { .data = NULL };
	struct xdr_netobj DR = { .data = NULL };
	unsigned int blocksize, offset;
	struct crypto_shash *tfm;
	int n, count, ret;

	 
	tfm = crypto_alloc_shash(gk5e->cksum_name, 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		goto out;
	}
	ret = crypto_shash_setkey(tfm, inkey->data, inkey->len);
	if (ret)
		goto out_free_tfm;

	blocksize = crypto_shash_digestsize(tfm);
	n = (outkey->len + blocksize - 1) / blocksize;

	 
	ret = -ENOMEM;
	step.len = blocksize;
	step.data = kzalloc(step.len, gfp_mask);
	if (!step.data)
		goto out_free_tfm;

	DR.len = blocksize * n;
	DR.data = kmalloc(DR.len, gfp_mask);
	if (!DR.data)
		goto out_free_tfm;

	 
	for (offset = 0, count = 1; count <= n; count++) {
		ret = krb5_cmac_Ki(tfm, constant, outkey->len, count, &step);
		if (ret)
			goto out_free_tfm;

		memcpy(DR.data + offset, step.data, blocksize);
		offset += blocksize;
	}

	 
	memcpy(outkey->data, DR.data, outkey->len);
	ret = 0;

out_free_tfm:
	crypto_free_shash(tfm);
out:
	kfree_sensitive(step.data);
	kfree_sensitive(DR.data);
	return ret;
}

 
static int
krb5_hmac_K1(struct crypto_shash *tfm, const struct xdr_netobj *label,
	     u32 outlen, struct xdr_netobj *K1)
{
	__be32 k = cpu_to_be32(outlen * 8);
	SHASH_DESC_ON_STACK(desc, tfm);
	__be32 one = cpu_to_be32(1);
	u8 zero = 0;
	int ret;

	desc->tfm = tfm;
	ret = crypto_shash_init(desc);
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, (u8 *)&one, sizeof(one));
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, label->data, label->len);
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, &zero, sizeof(zero));
	if (ret)
		goto out_err;
	ret = crypto_shash_update(desc, (u8 *)&k, sizeof(k));
	if (ret)
		goto out_err;
	ret = crypto_shash_final(desc, K1->data);
	if (ret)
		goto out_err;

out_err:
	shash_desc_zero(desc);
	return ret;
}

 
int
krb5_kdf_hmac_sha2(const struct gss_krb5_enctype *gk5e,
		   const struct xdr_netobj *inkey,
		   struct xdr_netobj *outkey,
		   const struct xdr_netobj *label,
		   gfp_t gfp_mask)
{
	struct crypto_shash *tfm;
	struct xdr_netobj K1 = {
		.data = NULL,
	};
	int ret;

	 
	tfm = crypto_alloc_shash(gk5e->cksum_name, 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		goto out;
	}
	ret = crypto_shash_setkey(tfm, inkey->data, inkey->len);
	if (ret)
		goto out_free_tfm;

	K1.len = crypto_shash_digestsize(tfm);
	K1.data = kmalloc(K1.len, gfp_mask);
	if (!K1.data) {
		ret = -ENOMEM;
		goto out_free_tfm;
	}

	ret = krb5_hmac_K1(tfm, label, outkey->len, &K1);
	if (ret)
		goto out_free_tfm;

	 
	memcpy(outkey->data, K1.data, outkey->len);

out_free_tfm:
	kfree_sensitive(K1.data);
	crypto_free_shash(tfm);
out:
	return ret;
}
