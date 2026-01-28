


#ifndef AES_GMAC_H
#define AES_GMAC_H

#include <linux/crypto.h>

#define GMAC_AAD_LEN	20
#define GMAC_MIC_LEN	16
#define GMAC_NONCE_LEN	12

struct crypto_aead *ieee80211_aes_gmac_key_setup(const u8 key[],
						 size_t key_len);
int ieee80211_aes_gmac(struct crypto_aead *tfm, const u8 *aad, u8 *nonce,
		       const u8 *data, size_t data_len, u8 *mic);
void ieee80211_aes_gmac_key_free(struct crypto_aead *tfm);

#endif 
