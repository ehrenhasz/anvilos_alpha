 
 

#include "includes.h"

#ifdef USE_SNTRUP761X25519

#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "sshkey.h"
#include "kex.h"
#include "sshbuf.h"
#include "digest.h"
#include "ssherr.h"

int
kex_kem_sntrup761x25519_keypair(struct kex *kex)
{
	struct sshbuf *buf = NULL;
	u_char *cp = NULL;
	size_t need;
	int r;

	if ((buf = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	need = crypto_kem_sntrup761_PUBLICKEYBYTES + CURVE25519_SIZE;
	if ((r = sshbuf_reserve(buf, need, &cp)) != 0)
		goto out;
	crypto_kem_sntrup761_keypair(cp, kex->sntrup761_client_key);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key sntrup761:", cp,
	    crypto_kem_sntrup761_PUBLICKEYBYTES);
#endif
	cp += crypto_kem_sntrup761_PUBLICKEYBYTES;
	kexc25519_keygen(kex->c25519_client_key, cp);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key c25519:", cp, CURVE25519_SIZE);
#endif
	kex->client_pub = buf;
	buf = NULL;
 out:
	sshbuf_free(buf);
	return r;
}

int
kex_kem_sntrup761x25519_enc(struct kex *kex,
   const struct sshbuf *client_blob, struct sshbuf **server_blobp,
   struct sshbuf **shared_secretp)
{
	struct sshbuf *server_blob = NULL;
	struct sshbuf *buf = NULL;
	const u_char *client_pub;
	u_char *kem_key, *ciphertext, *server_pub;
	u_char server_key[CURVE25519_SIZE];
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t need;
	int r;

	*server_blobp = NULL;
	*shared_secretp = NULL;

	 
	need = crypto_kem_sntrup761_PUBLICKEYBYTES + CURVE25519_SIZE;
	if (sshbuf_len(client_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	client_pub = sshbuf_ptr(client_blob);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key sntrup761:", client_pub,
	    crypto_kem_sntrup761_PUBLICKEYBYTES);
	dump_digest("client public key 25519:",
	    client_pub + crypto_kem_sntrup761_PUBLICKEYBYTES,
	    CURVE25519_SIZE);
#endif
	 
	 
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_reserve(buf, crypto_kem_sntrup761_BYTES,
	    &kem_key)) != 0)
		goto out;
	 
	if ((server_blob = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	need = crypto_kem_sntrup761_CIPHERTEXTBYTES + CURVE25519_SIZE;
	if ((r = sshbuf_reserve(server_blob, need, &ciphertext)) != 0)
		goto out;
	 
	crypto_kem_sntrup761_enc(ciphertext, kem_key, client_pub);
	 
	server_pub = ciphertext + crypto_kem_sntrup761_CIPHERTEXTBYTES;
	kexc25519_keygen(server_key, server_pub);
	 
	client_pub += crypto_kem_sntrup761_PUBLICKEYBYTES;
	if ((r = kexc25519_shared_key_ext(server_key, client_pub, buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf, hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("server public key 25519:", server_pub, CURVE25519_SIZE);
	dump_digest("server cipher text:", ciphertext,
	    crypto_kem_sntrup761_CIPHERTEXTBYTES);
	dump_digest("server kem key:", kem_key, crypto_kem_sntrup761_BYTES);
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	 
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	*server_blobp = server_blob;
	*shared_secretp = buf;
	server_blob = NULL;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(server_key, sizeof(server_key));
	sshbuf_free(server_blob);
	sshbuf_free(buf);
	return r;
}

int
kex_kem_sntrup761x25519_dec(struct kex *kex,
    const struct sshbuf *server_blob, struct sshbuf **shared_secretp)
{
	struct sshbuf *buf = NULL;
	u_char *kem_key = NULL;
	const u_char *ciphertext, *server_pub;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t need;
	int r, decoded;

	*shared_secretp = NULL;

	need = crypto_kem_sntrup761_CIPHERTEXTBYTES + CURVE25519_SIZE;
	if (sshbuf_len(server_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	ciphertext = sshbuf_ptr(server_blob);
	server_pub = ciphertext + crypto_kem_sntrup761_CIPHERTEXTBYTES;
#ifdef DEBUG_KEXECDH
	dump_digest("server cipher text:", ciphertext,
	    crypto_kem_sntrup761_CIPHERTEXTBYTES);
	dump_digest("server public key c25519:", server_pub, CURVE25519_SIZE);
#endif
	 
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_reserve(buf, crypto_kem_sntrup761_BYTES,
	    &kem_key)) != 0)
		goto out;
	decoded = crypto_kem_sntrup761_dec(kem_key, ciphertext,
	    kex->sntrup761_client_key);
	if ((r = kexc25519_shared_key_ext(kex->c25519_client_key, server_pub,
	    buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf, hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("client kem key:", kem_key, crypto_kem_sntrup761_BYTES);
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	if (decoded != 0) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	*shared_secretp = buf;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	sshbuf_free(buf);
	return r;
}

#else

#include "ssherr.h"

struct kex;
struct sshbuf;
struct sshkey;

int
kex_kem_sntrup761x25519_keypair(struct kex *kex)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int
kex_kem_sntrup761x25519_enc(struct kex *kex,
   const struct sshbuf *client_blob, struct sshbuf **server_blobp,
   struct sshbuf **shared_secretp)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int
kex_kem_sntrup761x25519_dec(struct kex *kex,
    const struct sshbuf *server_blob, struct sshbuf **shared_secretp)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}
#endif  
