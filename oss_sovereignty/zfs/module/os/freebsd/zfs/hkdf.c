#include <sys/dmu.h>
#include <sys/hkdf.h>
#include <sys/freebsd_crypto.h>
#include <sys/hkdf.h>
static int
hkdf_sha512_extract(uint8_t *salt, uint_t salt_len, uint8_t *key_material,
    uint_t km_len, uint8_t *out_buf)
{
	crypto_key_t key;
	key.ck_length = CRYPTO_BYTES2BITS(salt_len);
	key.ck_data = salt;
	crypto_mac(&key, key_material, km_len, out_buf, SHA512_DIGEST_LENGTH);
	return (0);
}
static int
hkdf_sha512_expand(uint8_t *extract_key, uint8_t *info, uint_t info_len,
    uint8_t *out_buf, uint_t out_len)
{
	struct hmac_ctx ctx;
	crypto_key_t key;
	uint_t i, T_len = 0, pos = 0;
	uint8_t c;
	uint_t N = (out_len + SHA512_DIGEST_LENGTH) / SHA512_DIGEST_LENGTH;
	uint8_t T[SHA512_DIGEST_LENGTH];
	if (N > 255)
		return (SET_ERROR(EINVAL));
	key.ck_length = CRYPTO_BYTES2BITS(SHA512_DIGEST_LENGTH);
	key.ck_data = extract_key;
	for (i = 1; i <= N; i++) {
		c = i;
		crypto_mac_init(&ctx, &key);
		crypto_mac_update(&ctx, T, T_len);
		crypto_mac_update(&ctx, info, info_len);
		crypto_mac_update(&ctx, &c, 1);
		crypto_mac_final(&ctx, T, SHA512_DIGEST_LENGTH);
		memcpy(out_buf + pos, T,
		    (i != N) ? SHA512_DIGEST_LENGTH : (out_len - pos));
		pos += SHA512_DIGEST_LENGTH;
	}
	return (0);
}
int
hkdf_sha512(uint8_t *key_material, uint_t km_len, uint8_t *salt,
    uint_t salt_len, uint8_t *info, uint_t info_len, uint8_t *output_key,
    uint_t out_len)
{
	int ret;
	uint8_t extract_key[SHA512_DIGEST_LENGTH];
	ret = hkdf_sha512_extract(salt, salt_len, key_material, km_len,
	    extract_key);
	if (ret != 0)
		return (ret);
	ret = hkdf_sha512_expand(extract_key, info, info_len, output_key,
	    out_len);
	if (ret != 0)
		return (ret);
	return (0);
}
