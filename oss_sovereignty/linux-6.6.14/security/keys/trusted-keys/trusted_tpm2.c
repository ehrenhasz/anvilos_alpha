
 

#include <linux/asn1_encoder.h>
#include <linux/oid_registry.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/tpm.h>
#include <linux/tpm_command.h>

#include <keys/trusted-type.h>
#include <keys/trusted_tpm.h>

#include <asm/unaligned.h>

#include "tpm2key.asn1.h"

static struct tpm2_hash tpm2_hash_map[] = {
	{HASH_ALGO_SHA1, TPM_ALG_SHA1},
	{HASH_ALGO_SHA256, TPM_ALG_SHA256},
	{HASH_ALGO_SHA384, TPM_ALG_SHA384},
	{HASH_ALGO_SHA512, TPM_ALG_SHA512},
	{HASH_ALGO_SM3_256, TPM_ALG_SM3_256},
};

static u32 tpm2key_oid[] = { 2, 23, 133, 10, 1, 5 };

static int tpm2_key_encode(struct trusted_key_payload *payload,
			   struct trusted_key_options *options,
			   u8 *src, u32 len)
{
	const int SCRATCH_SIZE = PAGE_SIZE;
	u8 *scratch = kmalloc(SCRATCH_SIZE, GFP_KERNEL);
	u8 *work = scratch, *work1;
	u8 *end_work = scratch + SCRATCH_SIZE;
	u8 *priv, *pub;
	u16 priv_len, pub_len;

	priv_len = get_unaligned_be16(src) + 2;
	priv = src;

	src += priv_len;

	pub_len = get_unaligned_be16(src) + 2;
	pub = src;

	if (!scratch)
		return -ENOMEM;

	work = asn1_encode_oid(work, end_work, tpm2key_oid,
			       asn1_oid_len(tpm2key_oid));

	if (options->blobauth_len == 0) {
		unsigned char bool[3], *w = bool;
		 
		w = asn1_encode_boolean(w, w + sizeof(bool), true);
		if (WARN(IS_ERR(w), "BUG: Boolean failed to encode"))
			return PTR_ERR(w);
		work = asn1_encode_tag(work, end_work, 0, bool, w - bool);
	}

	 
	if (WARN(work - scratch + pub_len + priv_len + 14 > SCRATCH_SIZE,
		 "BUG: scratch buffer is too small"))
		return -EINVAL;

	work = asn1_encode_integer(work, end_work, options->keyhandle);
	work = asn1_encode_octet_string(work, end_work, pub, pub_len);
	work = asn1_encode_octet_string(work, end_work, priv, priv_len);

	work1 = payload->blob;
	work1 = asn1_encode_sequence(work1, work1 + sizeof(payload->blob),
				     scratch, work - scratch);
	if (WARN(IS_ERR(work1), "BUG: ASN.1 encoder failed"))
		return PTR_ERR(work1);

	return work1 - payload->blob;
}

struct tpm2_key_context {
	u32 parent;
	const u8 *pub;
	u32 pub_len;
	const u8 *priv;
	u32 priv_len;
};

static int tpm2_key_decode(struct trusted_key_payload *payload,
			   struct trusted_key_options *options,
			   u8 **buf)
{
	int ret;
	struct tpm2_key_context ctx;
	u8 *blob;

	memset(&ctx, 0, sizeof(ctx));

	ret = asn1_ber_decoder(&tpm2key_decoder, &ctx, payload->blob,
			       payload->blob_len);
	if (ret < 0)
		return ret;

	if (ctx.priv_len + ctx.pub_len > MAX_BLOB_SIZE)
		return -EINVAL;

	blob = kmalloc(ctx.priv_len + ctx.pub_len + 4, GFP_KERNEL);
	if (!blob)
		return -ENOMEM;

	*buf = blob;
	options->keyhandle = ctx.parent;

	memcpy(blob, ctx.priv, ctx.priv_len);
	blob += ctx.priv_len;

	memcpy(blob, ctx.pub, ctx.pub_len);

	return 0;
}

int tpm2_key_parent(void *context, size_t hdrlen,
		  unsigned char tag,
		  const void *value, size_t vlen)
{
	struct tpm2_key_context *ctx = context;
	const u8 *v = value;
	int i;

	ctx->parent = 0;
	for (i = 0; i < vlen; i++) {
		ctx->parent <<= 8;
		ctx->parent |= v[i];
	}

	return 0;
}

int tpm2_key_type(void *context, size_t hdrlen,
		unsigned char tag,
		const void *value, size_t vlen)
{
	enum OID oid = look_up_OID(value, vlen);

	if (oid != OID_TPMSealedData) {
		char buffer[50];

		sprint_oid(value, vlen, buffer, sizeof(buffer));
		pr_debug("OID is \"%s\" which is not TPMSealedData\n",
			 buffer);
		return -EINVAL;
	}

	return 0;
}

int tpm2_key_pub(void *context, size_t hdrlen,
	       unsigned char tag,
	       const void *value, size_t vlen)
{
	struct tpm2_key_context *ctx = context;

	ctx->pub = value;
	ctx->pub_len = vlen;

	return 0;
}

int tpm2_key_priv(void *context, size_t hdrlen,
		unsigned char tag,
		const void *value, size_t vlen)
{
	struct tpm2_key_context *ctx = context;

	ctx->priv = value;
	ctx->priv_len = vlen;

	return 0;
}

 
static void tpm2_buf_append_auth(struct tpm_buf *buf, u32 session_handle,
				 const u8 *nonce, u16 nonce_len,
				 u8 attributes,
				 const u8 *hmac, u16 hmac_len)
{
	tpm_buf_append_u32(buf, 9 + nonce_len + hmac_len);
	tpm_buf_append_u32(buf, session_handle);
	tpm_buf_append_u16(buf, nonce_len);

	if (nonce && nonce_len)
		tpm_buf_append(buf, nonce, nonce_len);

	tpm_buf_append_u8(buf, attributes);
	tpm_buf_append_u16(buf, hmac_len);

	if (hmac && hmac_len)
		tpm_buf_append(buf, hmac, hmac_len);
}

 
int tpm2_seal_trusted(struct tpm_chip *chip,
		      struct trusted_key_payload *payload,
		      struct trusted_key_options *options)
{
	int blob_len = 0;
	struct tpm_buf buf;
	u32 hash;
	u32 flags;
	int i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(tpm2_hash_map); i++) {
		if (options->hash == tpm2_hash_map[i].crypto_id) {
			hash = tpm2_hash_map[i].tpm_id;
			break;
		}
	}

	if (i == ARRAY_SIZE(tpm2_hash_map))
		return -EINVAL;

	if (!options->keyhandle)
		return -EINVAL;

	rc = tpm_try_get_ops(chip);
	if (rc)
		return rc;

	rc = tpm_buf_init(&buf, TPM2_ST_SESSIONS, TPM2_CC_CREATE);
	if (rc) {
		tpm_put_ops(chip);
		return rc;
	}

	tpm_buf_append_u32(&buf, options->keyhandle);
	tpm2_buf_append_auth(&buf, TPM2_RS_PW,
			     NULL  , 0,
			     0  ,
			     options->keyauth  ,
			     TPM_DIGEST_SIZE);

	 
	tpm_buf_append_u16(&buf, 4 + options->blobauth_len + payload->key_len);

	tpm_buf_append_u16(&buf, options->blobauth_len);
	if (options->blobauth_len)
		tpm_buf_append(&buf, options->blobauth, options->blobauth_len);

	tpm_buf_append_u16(&buf, payload->key_len);
	tpm_buf_append(&buf, payload->key, payload->key_len);

	 
	tpm_buf_append_u16(&buf, 14 + options->policydigest_len);
	tpm_buf_append_u16(&buf, TPM_ALG_KEYEDHASH);
	tpm_buf_append_u16(&buf, hash);

	 
	flags = 0;
	flags |= options->policydigest_len ? 0 : TPM2_OA_USER_WITH_AUTH;
	flags |= payload->migratable ? 0 : (TPM2_OA_FIXED_TPM |
					    TPM2_OA_FIXED_PARENT);
	tpm_buf_append_u32(&buf, flags);

	 
	tpm_buf_append_u16(&buf, options->policydigest_len);
	if (options->policydigest_len)
		tpm_buf_append(&buf, options->policydigest,
			       options->policydigest_len);

	 
	tpm_buf_append_u16(&buf, TPM_ALG_NULL);
	tpm_buf_append_u16(&buf, 0);

	 
	tpm_buf_append_u16(&buf, 0);

	 
	tpm_buf_append_u32(&buf, 0);

	if (buf.flags & TPM_BUF_OVERFLOW) {
		rc = -E2BIG;
		goto out;
	}

	rc = tpm_transmit_cmd(chip, &buf, 4, "sealing data");
	if (rc)
		goto out;

	blob_len = be32_to_cpup((__be32 *) &buf.data[TPM_HEADER_SIZE]);
	if (blob_len > MAX_BLOB_SIZE) {
		rc = -E2BIG;
		goto out;
	}
	if (tpm_buf_length(&buf) < TPM_HEADER_SIZE + 4 + blob_len) {
		rc = -EFAULT;
		goto out;
	}

	blob_len = tpm2_key_encode(payload, options,
				   &buf.data[TPM_HEADER_SIZE + 4],
				   blob_len);

out:
	tpm_buf_destroy(&buf);

	if (rc > 0) {
		if (tpm2_rc_value(rc) == TPM2_RC_HASH)
			rc = -EINVAL;
		else
			rc = -EPERM;
	}
	if (blob_len < 0)
		rc = blob_len;
	else
		payload->blob_len = blob_len;

	tpm_put_ops(chip);
	return rc;
}

 
static int tpm2_load_cmd(struct tpm_chip *chip,
			 struct trusted_key_payload *payload,
			 struct trusted_key_options *options,
			 u32 *blob_handle)
{
	struct tpm_buf buf;
	unsigned int private_len;
	unsigned int public_len;
	unsigned int blob_len;
	u8 *blob, *pub;
	int rc;
	u32 attrs;

	rc = tpm2_key_decode(payload, options, &blob);
	if (rc) {
		 
		blob = payload->blob;
		payload->old_format = 1;
	}

	 
	if (!options->keyhandle)
		return -EINVAL;

	 
	if (payload->blob_len < 4)
		return -EINVAL;

	private_len = get_unaligned_be16(blob);

	 
	if (private_len + 2 + 2 > (payload->blob_len))
		return -E2BIG;

	public_len = get_unaligned_be16(blob + 2 + private_len);
	if (private_len + 2 + public_len + 2 > payload->blob_len)
		return -E2BIG;

	pub = blob + 2 + private_len + 2;
	 
	attrs = get_unaligned_be32(pub + 4);

	if ((attrs & (TPM2_OA_FIXED_TPM | TPM2_OA_FIXED_PARENT)) ==
	    (TPM2_OA_FIXED_TPM | TPM2_OA_FIXED_PARENT))
		payload->migratable = 0;
	else
		payload->migratable = 1;

	blob_len = private_len + public_len + 4;
	if (blob_len > payload->blob_len)
		return -E2BIG;

	rc = tpm_buf_init(&buf, TPM2_ST_SESSIONS, TPM2_CC_LOAD);
	if (rc)
		return rc;

	tpm_buf_append_u32(&buf, options->keyhandle);
	tpm2_buf_append_auth(&buf, TPM2_RS_PW,
			     NULL  , 0,
			     0  ,
			     options->keyauth  ,
			     TPM_DIGEST_SIZE);

	tpm_buf_append(&buf, blob, blob_len);

	if (buf.flags & TPM_BUF_OVERFLOW) {
		rc = -E2BIG;
		goto out;
	}

	rc = tpm_transmit_cmd(chip, &buf, 4, "loading blob");
	if (!rc)
		*blob_handle = be32_to_cpup(
			(__be32 *) &buf.data[TPM_HEADER_SIZE]);

out:
	if (blob != payload->blob)
		kfree(blob);
	tpm_buf_destroy(&buf);

	if (rc > 0)
		rc = -EPERM;

	return rc;
}

 
static int tpm2_unseal_cmd(struct tpm_chip *chip,
			   struct trusted_key_payload *payload,
			   struct trusted_key_options *options,
			   u32 blob_handle)
{
	struct tpm_buf buf;
	u16 data_len;
	u8 *data;
	int rc;

	rc = tpm_buf_init(&buf, TPM2_ST_SESSIONS, TPM2_CC_UNSEAL);
	if (rc)
		return rc;

	tpm_buf_append_u32(&buf, blob_handle);
	tpm2_buf_append_auth(&buf,
			     options->policyhandle ?
			     options->policyhandle : TPM2_RS_PW,
			     NULL  , 0,
			     TPM2_SA_CONTINUE_SESSION,
			     options->blobauth  ,
			     options->blobauth_len);

	rc = tpm_transmit_cmd(chip, &buf, 6, "unsealing");
	if (rc > 0)
		rc = -EPERM;

	if (!rc) {
		data_len = be16_to_cpup(
			(__be16 *) &buf.data[TPM_HEADER_SIZE + 4]);
		if (data_len < MIN_KEY_SIZE ||  data_len > MAX_KEY_SIZE) {
			rc = -EFAULT;
			goto out;
		}

		if (tpm_buf_length(&buf) < TPM_HEADER_SIZE + 6 + data_len) {
			rc = -EFAULT;
			goto out;
		}
		data = &buf.data[TPM_HEADER_SIZE + 6];

		if (payload->old_format) {
			 
			memcpy(payload->key, data, data_len - 1);
			payload->key_len = data_len - 1;
			payload->migratable = data[data_len - 1];
		} else {
			 
			memcpy(payload->key, data, data_len);
			payload->key_len = data_len;
		}
	}

out:
	tpm_buf_destroy(&buf);
	return rc;
}

 
int tpm2_unseal_trusted(struct tpm_chip *chip,
			struct trusted_key_payload *payload,
			struct trusted_key_options *options)
{
	u32 blob_handle;
	int rc;

	rc = tpm_try_get_ops(chip);
	if (rc)
		return rc;

	rc = tpm2_load_cmd(chip, payload, options, &blob_handle);
	if (rc)
		goto out;

	rc = tpm2_unseal_cmd(chip, payload, options, blob_handle);
	tpm2_flush_context(chip, blob_handle);

out:
	tpm_put_ops(chip);

	return rc;
}
