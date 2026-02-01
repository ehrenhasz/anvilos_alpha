 
 

 

#include "includes.h"

#include <sys/types.h>

#ifdef WITH_OPENSSL
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#endif

#include <string.h>
#include <stdio.h>  

#include "openbsd-compat/openssl-compat.h"

#include "sshbuf.h"
#include "ssherr.h"
#include "digest.h"
#define SSHKEY_INTERNAL
#include "sshkey.h"

#ifndef OPENSSL_HAS_ECC
 
int
ssh_ecdsa_sk_verify(const struct sshkey *key,
    const u_char *signature, size_t signaturelen,
    const u_char *data, size_t datalen, u_int compat,
    struct sshkey_sig_details **detailsp)
{
	return SSH_ERR_FEATURE_UNSUPPORTED;
}
#else  

 
extern struct sshkey_impl_funcs sshkey_ecdsa_funcs;

static void
ssh_ecdsa_sk_cleanup(struct sshkey *k)
{
	sshkey_sk_cleanup(k);
	sshkey_ecdsa_funcs.cleanup(k);
}

static int
ssh_ecdsa_sk_equal(const struct sshkey *a, const struct sshkey *b)
{
	if (!sshkey_sk_fields_equal(a, b))
		return 0;
	if (!sshkey_ecdsa_funcs.equal(a, b))
		return 0;
	return 1;
}

static int
ssh_ecdsa_sk_serialize_public(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;

	if ((r = sshkey_ecdsa_funcs.serialize_public(key, b, opts)) != 0)
		return r;
	if ((r = sshkey_serialize_sk(key, b)) != 0)
		return r;

	return 0;
}

static int
ssh_ecdsa_sk_serialize_private(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;

	if (!sshkey_is_cert(key)) {
		if ((r = sshkey_ecdsa_funcs.serialize_public(key,
		    b, opts)) != 0)
			return r;
	}
	if ((r = sshkey_serialize_private_sk(key, b)) != 0)
		return r;

	return 0;
}

static int
ssh_ecdsa_sk_copy_public(const struct sshkey *from, struct sshkey *to)
{
	int r;

	if ((r = sshkey_ecdsa_funcs.copy_public(from, to)) != 0)
		return r;
	if ((r = sshkey_copy_public_sk(from, to)) != 0)
		return r;
	return 0;
}

static int
ssh_ecdsa_sk_deserialize_public(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int r;

	if ((r = sshkey_ecdsa_funcs.deserialize_public(ktype, b, key)) != 0)
		return r;
	if ((r = sshkey_deserialize_sk(b, key)) != 0)
		return r;
	return 0;
}

static int
ssh_ecdsa_sk_deserialize_private(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int r;

	if (!sshkey_is_cert(key)) {
		if ((r = sshkey_ecdsa_funcs.deserialize_public(ktype,
		    b, key)) != 0)
			return r;
	}
	if ((r = sshkey_private_deserialize_sk(b, key)) != 0)
		return r;

	return 0;
}

 
static int
webauthn_check_prepare_hash(const u_char *data, size_t datalen,
    const char *origin, const struct sshbuf *wrapper,
    uint8_t flags, const struct sshbuf *extensions,
    u_char *msghash, size_t msghashlen)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *chall = NULL, *m = NULL;

	if ((m = sshbuf_new()) == NULL ||
	    (chall = sshbuf_from(data, datalen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	 
	if (strchr(origin, '\"') != NULL ||
	    (flags & 0x40) != 0   ||
	    ((flags & 0x80) == 0  ) != (sshbuf_len(extensions) == 0)) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	 
#define WEBAUTHN_0	"{\"type\":\"webauthn.get\",\"challenge\":\""
#define WEBAUTHN_1	"\",\"origin\":\""
#define WEBAUTHN_2	"\""
	if ((r = sshbuf_put(m, WEBAUTHN_0, sizeof(WEBAUTHN_0) - 1)) != 0 ||
	    (r = sshbuf_dtourlb64(chall, m, 0)) != 0 ||
	    (r = sshbuf_put(m, WEBAUTHN_1, sizeof(WEBAUTHN_1) - 1)) != 0 ||
	    (r = sshbuf_put(m, origin, strlen(origin))) != 0 ||
	    (r = sshbuf_put(m, WEBAUTHN_2, sizeof(WEBAUTHN_2) - 1)) != 0)
		goto out;
#ifdef DEBUG_SK
	fprintf(stderr, "%s: received origin: %s\n", __func__, origin);
	fprintf(stderr, "%s: received clientData:\n", __func__);
	sshbuf_dump(wrapper, stderr);
	fprintf(stderr, "%s: expected clientData premable:\n", __func__);
	sshbuf_dump(m, stderr);
#endif
	 
	if ((r = sshbuf_cmp(wrapper, 0, sshbuf_ptr(m), sshbuf_len(m))) != 0)
		goto out;

	 
	if ((r = ssh_digest_buffer(SSH_DIGEST_SHA256, wrapper,
	    msghash, msghashlen)) != 0)
		goto out;

	 
	r = 0;
 out:
	sshbuf_free(chall);
	sshbuf_free(m);
	return r;
}

static int
ssh_ecdsa_sk_verify(const struct sshkey *key,
    const u_char *sig, size_t siglen,
    const u_char *data, size_t dlen, const char *alg, u_int compat,
    struct sshkey_sig_details **detailsp)
{
	ECDSA_SIG *esig = NULL;
	BIGNUM *sig_r = NULL, *sig_s = NULL;
	u_char sig_flags;
	u_char msghash[32], apphash[32], sighash[32];
	u_int sig_counter;
	int is_webauthn = 0, ret = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *b = NULL, *sigbuf = NULL, *original_signed = NULL;
	struct sshbuf *webauthn_wrapper = NULL, *webauthn_exts = NULL;
	char *ktype = NULL, *webauthn_origin = NULL;
	struct sshkey_sig_details *details = NULL;
#ifdef DEBUG_SK
	char *tmp = NULL;
#endif

	if (detailsp != NULL)
		*detailsp = NULL;
	if (key == NULL || key->ecdsa == NULL ||
	    sshkey_type_plain(key->type) != KEY_ECDSA_SK ||
	    sig == NULL || siglen == 0)
		return SSH_ERR_INVALID_ARGUMENT;

	if (key->ecdsa_nid != NID_X9_62_prime256v1)
		return SSH_ERR_INTERNAL_ERROR;

	 
	if ((b = sshbuf_from(sig, siglen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((details = calloc(1, sizeof(*details))) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (sshbuf_get_cstring(b, &ktype, NULL) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (strcmp(ktype, "webauthn-sk-ecdsa-sha2-nistp256@openssh.com") == 0)
		is_webauthn = 1;
	else if (strcmp(ktype, "sk-ecdsa-sha2-nistp256@openssh.com") != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_froms(b, &sigbuf) != 0 ||
	    sshbuf_get_u8(b, &sig_flags) != 0 ||
	    sshbuf_get_u32(b, &sig_counter) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (is_webauthn) {
		if (sshbuf_get_cstring(b, &webauthn_origin, NULL) != 0 ||
		    sshbuf_froms(b, &webauthn_wrapper) != 0 ||
		    sshbuf_froms(b, &webauthn_exts) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}
	if (sshbuf_len(b) != 0) {
		ret = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}

	 
	if (sshbuf_get_bignum2(sigbuf, &sig_r) != 0 ||
	    sshbuf_get_bignum2(sigbuf, &sig_s) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(sigbuf) != 0) {
		ret = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}

#ifdef DEBUG_SK
	fprintf(stderr, "%s: data: (len %zu)\n", __func__, datalen);
	 
	fprintf(stderr, "%s: sig_r: %s\n", __func__, (tmp = BN_bn2hex(sig_r)));
	free(tmp);
	fprintf(stderr, "%s: sig_s: %s\n", __func__, (tmp = BN_bn2hex(sig_s)));
	free(tmp);
	fprintf(stderr, "%s: sig_flags = 0x%02x, sig_counter = %u\n",
	    __func__, sig_flags, sig_counter);
	if (is_webauthn) {
		fprintf(stderr, "%s: webauthn origin: %s\n", __func__,
		    webauthn_origin);
		fprintf(stderr, "%s: webauthn_wrapper:\n", __func__);
		sshbuf_dump(webauthn_wrapper, stderr);
	}
#endif
	if ((esig = ECDSA_SIG_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (!ECDSA_SIG_set0(esig, sig_r, sig_s)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	sig_r = sig_s = NULL;  

	 
	if ((original_signed = sshbuf_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (is_webauthn) {
		if ((ret = webauthn_check_prepare_hash(data, dlen,
		    webauthn_origin, webauthn_wrapper, sig_flags, webauthn_exts,
		    msghash, sizeof(msghash))) != 0)
			goto out;
	} else if ((ret = ssh_digest_memory(SSH_DIGEST_SHA256, data, dlen,
	    msghash, sizeof(msghash))) != 0)
		goto out;
	 
	if ((ret = ssh_digest_memory(SSH_DIGEST_SHA256, key->sk_application,
	    strlen(key->sk_application), apphash, sizeof(apphash))) != 0)
		goto out;
#ifdef DEBUG_SK
	fprintf(stderr, "%s: hashed application:\n", __func__);
	sshbuf_dump_data(apphash, sizeof(apphash), stderr);
	fprintf(stderr, "%s: hashed message:\n", __func__);
	sshbuf_dump_data(msghash, sizeof(msghash), stderr);
#endif
	if ((ret = sshbuf_put(original_signed,
	    apphash, sizeof(apphash))) != 0 ||
	    (ret = sshbuf_put_u8(original_signed, sig_flags)) != 0 ||
	    (ret = sshbuf_put_u32(original_signed, sig_counter)) != 0 ||
	    (ret = sshbuf_putb(original_signed, webauthn_exts)) != 0 ||
	    (ret = sshbuf_put(original_signed, msghash, sizeof(msghash))) != 0)
		goto out;
	 
	if ((ret = ssh_digest_buffer(SSH_DIGEST_SHA256, original_signed,
	    sighash, sizeof(sighash))) != 0)
		goto out;
	details->sk_counter = sig_counter;
	details->sk_flags = sig_flags;
#ifdef DEBUG_SK
	fprintf(stderr, "%s: signed buf:\n", __func__);
	sshbuf_dump(original_signed, stderr);
	fprintf(stderr, "%s: signed hash:\n", __func__);
	sshbuf_dump_data(sighash, sizeof(sighash), stderr);
#endif

	 
	switch (ECDSA_do_verify(sighash, sizeof(sighash), esig, key->ecdsa)) {
	case 1:
		ret = 0;
		break;
	case 0:
		ret = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	default:
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	 
	if (detailsp != NULL) {
		*detailsp = details;
		details = NULL;
	}
 out:
	explicit_bzero(&sig_flags, sizeof(sig_flags));
	explicit_bzero(&sig_counter, sizeof(sig_counter));
	explicit_bzero(msghash, sizeof(msghash));
	explicit_bzero(sighash, sizeof(msghash));
	explicit_bzero(apphash, sizeof(apphash));
	sshkey_sig_details_free(details);
	sshbuf_free(webauthn_wrapper);
	sshbuf_free(webauthn_exts);
	free(webauthn_origin);
	sshbuf_free(original_signed);
	sshbuf_free(sigbuf);
	sshbuf_free(b);
	ECDSA_SIG_free(esig);
	BN_clear_free(sig_r);
	BN_clear_free(sig_s);
	free(ktype);
	return ret;
}

static const struct sshkey_impl_funcs sshkey_ecdsa_sk_funcs = {
	 		NULL,
	 		NULL,
	 	ssh_ecdsa_sk_cleanup,
	 		ssh_ecdsa_sk_equal,
	  ssh_ecdsa_sk_serialize_public,
	  ssh_ecdsa_sk_deserialize_public,
	  ssh_ecdsa_sk_serialize_private,
	  ssh_ecdsa_sk_deserialize_private,
	 	NULL,
	 	ssh_ecdsa_sk_copy_public,
	 		NULL,
	 		ssh_ecdsa_sk_verify,
};

const struct sshkey_impl sshkey_ecdsa_sk_impl = {
	 		"sk-ecdsa-sha2-nistp256@openssh.com",
	 	"ECDSA-SK",
	 		NULL,
	 		KEY_ECDSA_SK,
	 		NID_X9_62_prime256v1,
	 		0,
	 	0,
	 	256,
	 		&sshkey_ecdsa_sk_funcs,
};

const struct sshkey_impl sshkey_ecdsa_sk_cert_impl = {
	 		"sk-ecdsa-sha2-nistp256-cert-v01@openssh.com",
	 	"ECDSA-SK-CERT",
	 		NULL,
	 		KEY_ECDSA_SK_CERT,
	 		NID_X9_62_prime256v1,
	 		1,
	 	0,
	 	256,
	 		&sshkey_ecdsa_sk_funcs,
};

const struct sshkey_impl sshkey_ecdsa_sk_webauthn_impl = {
	 		"webauthn-sk-ecdsa-sha2-nistp256@openssh.com",
	 	"ECDSA-SK",
	 		NULL,
	 		KEY_ECDSA_SK,
	 		NID_X9_62_prime256v1,
	 		0,
	 	1,
	 	256,
	 		&sshkey_ecdsa_sk_funcs,
};

#endif  
