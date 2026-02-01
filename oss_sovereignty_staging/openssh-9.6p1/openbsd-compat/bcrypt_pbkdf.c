 
 

 

 

#include "includes.h"

#ifndef HAVE_BCRYPT_PBKDF

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <string.h>

#ifdef HAVE_BLF_H
# include <blf.h>
#endif

#include "crypto_api.h"
#ifdef SHA512_DIGEST_LENGTH
# undef SHA512_DIGEST_LENGTH
#endif
#define SHA512_DIGEST_LENGTH crypto_hash_sha512_BYTES

#define	MINIMUM(a,b) (((a) < (b)) ? (a) : (b))

 

#define BCRYPT_WORDS 8
#define BCRYPT_HASHSIZE (BCRYPT_WORDS * 4)

static void
bcrypt_hash(uint8_t *sha2pass, uint8_t *sha2salt, uint8_t *out)
{
	blf_ctx state;
	uint8_t ciphertext[BCRYPT_HASHSIZE] =
	    "OxychromaticBlowfishSwatDynamite";
	uint32_t cdata[BCRYPT_WORDS];
	int i;
	uint16_t j;
	size_t shalen = SHA512_DIGEST_LENGTH;

	 
	Blowfish_initstate(&state);
	Blowfish_expandstate(&state, sha2salt, shalen, sha2pass, shalen);
	for (i = 0; i < 64; i++) {
		Blowfish_expand0state(&state, sha2salt, shalen);
		Blowfish_expand0state(&state, sha2pass, shalen);
	}

	 
	j = 0;
	for (i = 0; i < BCRYPT_WORDS; i++)
		cdata[i] = Blowfish_stream2word(ciphertext, sizeof(ciphertext),
		    &j);
	for (i = 0; i < 64; i++)
		blf_enc(&state, cdata, BCRYPT_WORDS / 2);

	 
	for (i = 0; i < BCRYPT_WORDS; i++) {
		out[4 * i + 3] = (cdata[i] >> 24) & 0xff;
		out[4 * i + 2] = (cdata[i] >> 16) & 0xff;
		out[4 * i + 1] = (cdata[i] >> 8) & 0xff;
		out[4 * i + 0] = cdata[i] & 0xff;
	}

	 
	explicit_bzero(ciphertext, sizeof(ciphertext));
	explicit_bzero(cdata, sizeof(cdata));
	explicit_bzero(&state, sizeof(state));
}

int
bcrypt_pbkdf(const char *pass, size_t passlen, const uint8_t *salt, size_t saltlen,
    uint8_t *key, size_t keylen, unsigned int rounds)
{
	uint8_t sha2pass[SHA512_DIGEST_LENGTH];
	uint8_t sha2salt[SHA512_DIGEST_LENGTH];
	uint8_t out[BCRYPT_HASHSIZE];
	uint8_t tmpout[BCRYPT_HASHSIZE];
	uint8_t *countsalt;
	size_t i, j, amt, stride;
	uint32_t count;
	size_t origkeylen = keylen;

	 
	if (rounds < 1)
		goto bad;
	if (passlen == 0 || saltlen == 0 || keylen == 0 ||
	    keylen > sizeof(out) * sizeof(out) || saltlen > 1<<20)
		goto bad;
	if ((countsalt = calloc(1, saltlen + 4)) == NULL)
		goto bad;
	stride = (keylen + sizeof(out) - 1) / sizeof(out);
	amt = (keylen + stride - 1) / stride;

	memcpy(countsalt, salt, saltlen);

	 
	crypto_hash_sha512(sha2pass, pass, passlen);

	 
	for (count = 1; keylen > 0; count++) {
		countsalt[saltlen + 0] = (count >> 24) & 0xff;
		countsalt[saltlen + 1] = (count >> 16) & 0xff;
		countsalt[saltlen + 2] = (count >> 8) & 0xff;
		countsalt[saltlen + 3] = count & 0xff;

		 
		crypto_hash_sha512(sha2salt, countsalt, saltlen + 4);

		bcrypt_hash(sha2pass, sha2salt, tmpout);
		memcpy(out, tmpout, sizeof(out));

		for (i = 1; i < rounds; i++) {
			 
			crypto_hash_sha512(sha2salt, tmpout, sizeof(tmpout));
			bcrypt_hash(sha2pass, sha2salt, tmpout);
			for (j = 0; j < sizeof(out); j++)
				out[j] ^= tmpout[j];
		}

		 
		amt = MINIMUM(amt, keylen);
		for (i = 0; i < amt; i++) {
			size_t dest = i * stride + (count - 1);
			if (dest >= origkeylen)
				break;
			key[dest] = out[i];
		}
		keylen -= i;
	}

	 
	freezero(countsalt, saltlen + 4);
	explicit_bzero(out, sizeof(out));
	explicit_bzero(tmpout, sizeof(tmpout));

	return 0;

bad:
	 
	arc4random_buf(key, keylen);
	return -1;
}
#endif  
