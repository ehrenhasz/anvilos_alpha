
 

#include <crypto/hash.h>
#include <crypto/sha2.h>

#include "fscrypt_private.h"

 
#define HKDF_HMAC_ALG		"hmac(sha512)"
#define HKDF_HASHLEN		SHA512_DIGEST_SIZE

 

 
static int hkdf_extract(struct crypto_shash *hmac_tfm, const u8 *ikm,
			unsigned int ikmlen, u8 prk[HKDF_HASHLEN])
{
	static const u8 default_salt[HKDF_HASHLEN];
	int err;

	err = crypto_shash_setkey(hmac_tfm, default_salt, HKDF_HASHLEN);
	if (err)
		return err;

	return crypto_shash_tfm_digest(hmac_tfm, ikm, ikmlen, prk);
}

 
int fscrypt_init_hkdf(struct fscrypt_hkdf *hkdf, const u8 *master_key,
		      unsigned int master_key_size)
{
	struct crypto_shash *hmac_tfm;
	u8 prk[HKDF_HASHLEN];
	int err;

	hmac_tfm = crypto_alloc_shash(HKDF_HMAC_ALG, 0, 0);
	if (IS_ERR(hmac_tfm)) {
		fscrypt_err(NULL, "Error allocating " HKDF_HMAC_ALG ": %ld",
			    PTR_ERR(hmac_tfm));
		return PTR_ERR(hmac_tfm);
	}

	if (WARN_ON_ONCE(crypto_shash_digestsize(hmac_tfm) != sizeof(prk))) {
		err = -EINVAL;
		goto err_free_tfm;
	}

	err = hkdf_extract(hmac_tfm, master_key, master_key_size, prk);
	if (err)
		goto err_free_tfm;

	err = crypto_shash_setkey(hmac_tfm, prk, sizeof(prk));
	if (err)
		goto err_free_tfm;

	hkdf->hmac_tfm = hmac_tfm;
	goto out;

err_free_tfm:
	crypto_free_shash(hmac_tfm);
out:
	memzero_explicit(prk, sizeof(prk));
	return err;
}

 
int fscrypt_hkdf_expand(const struct fscrypt_hkdf *hkdf, u8 context,
			const u8 *info, unsigned int infolen,
			u8 *okm, unsigned int okmlen)
{
	SHASH_DESC_ON_STACK(desc, hkdf->hmac_tfm);
	u8 prefix[9];
	unsigned int i;
	int err;
	const u8 *prev = NULL;
	u8 counter = 1;
	u8 tmp[HKDF_HASHLEN];

	if (WARN_ON_ONCE(okmlen > 255 * HKDF_HASHLEN))
		return -EINVAL;

	desc->tfm = hkdf->hmac_tfm;

	memcpy(prefix, "fscrypt\0", 8);
	prefix[8] = context;

	for (i = 0; i < okmlen; i += HKDF_HASHLEN) {

		err = crypto_shash_init(desc);
		if (err)
			goto out;

		if (prev) {
			err = crypto_shash_update(desc, prev, HKDF_HASHLEN);
			if (err)
				goto out;
		}

		err = crypto_shash_update(desc, prefix, sizeof(prefix));
		if (err)
			goto out;

		err = crypto_shash_update(desc, info, infolen);
		if (err)
			goto out;

		BUILD_BUG_ON(sizeof(counter) != 1);
		if (okmlen - i < HKDF_HASHLEN) {
			err = crypto_shash_finup(desc, &counter, 1, tmp);
			if (err)
				goto out;
			memcpy(&okm[i], tmp, okmlen - i);
			memzero_explicit(tmp, sizeof(tmp));
		} else {
			err = crypto_shash_finup(desc, &counter, 1, &okm[i]);
			if (err)
				goto out;
		}
		counter++;
		prev = &okm[i];
	}
	err = 0;
out:
	if (unlikely(err))
		memzero_explicit(okm, okmlen);  
	shash_desc_zero(desc);
	return err;
}

void fscrypt_destroy_hkdf(struct fscrypt_hkdf *hkdf)
{
	crypto_free_shash(hkdf->hmac_tfm);
}
