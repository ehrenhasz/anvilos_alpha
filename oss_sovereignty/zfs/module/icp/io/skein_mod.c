#include <sys/crypto/common.h>
#include <sys/crypto/icp.h>
#include <sys/crypto/spi.h>
#include <sys/sysmacros.h>
#define	SKEIN_MODULE_IMPL
#include <sys/skein.h>
static const crypto_mech_info_t skein_mech_info_tab[] = {
	{CKM_SKEIN_256, SKEIN_256_MECH_INFO_TYPE,
	    CRYPTO_FG_DIGEST | CRYPTO_FG_DIGEST_ATOMIC},
	{CKM_SKEIN_256_MAC, SKEIN_256_MAC_MECH_INFO_TYPE,
	    CRYPTO_FG_MAC | CRYPTO_FG_MAC_ATOMIC},
	{CKM_SKEIN_512, SKEIN_512_MECH_INFO_TYPE,
	    CRYPTO_FG_DIGEST | CRYPTO_FG_DIGEST_ATOMIC},
	{CKM_SKEIN_512_MAC, SKEIN_512_MAC_MECH_INFO_TYPE,
	    CRYPTO_FG_MAC | CRYPTO_FG_MAC_ATOMIC},
	{CKM_SKEIN1024, SKEIN1024_MECH_INFO_TYPE,
	    CRYPTO_FG_DIGEST | CRYPTO_FG_DIGEST_ATOMIC},
	{CKM_SKEIN1024_MAC, SKEIN1024_MAC_MECH_INFO_TYPE,
	    CRYPTO_FG_MAC | CRYPTO_FG_MAC_ATOMIC},
};
static int skein_digest_init(crypto_ctx_t *, crypto_mechanism_t *);
static int skein_digest(crypto_ctx_t *, crypto_data_t *, crypto_data_t *);
static int skein_update(crypto_ctx_t *, crypto_data_t *);
static int skein_final(crypto_ctx_t *, crypto_data_t *);
static int skein_digest_atomic(crypto_mechanism_t *, crypto_data_t *,
    crypto_data_t *);
static const crypto_digest_ops_t skein_digest_ops = {
	.digest_init = skein_digest_init,
	.digest = skein_digest,
	.digest_update = skein_update,
	.digest_final = skein_final,
	.digest_atomic = skein_digest_atomic
};
static int skein_mac_init(crypto_ctx_t *, crypto_mechanism_t *, crypto_key_t *,
    crypto_spi_ctx_template_t);
static int skein_mac_atomic(crypto_mechanism_t *, crypto_key_t *,
    crypto_data_t *, crypto_data_t *, crypto_spi_ctx_template_t);
static const crypto_mac_ops_t skein_mac_ops = {
	.mac_init = skein_mac_init,
	.mac = NULL,
	.mac_update = skein_update,  
	.mac_final = skein_final,    
	.mac_atomic = skein_mac_atomic,
	.mac_verify_atomic = NULL
};
static int skein_create_ctx_template(crypto_mechanism_t *, crypto_key_t *,
    crypto_spi_ctx_template_t *, size_t *);
static int skein_free_context(crypto_ctx_t *);
static const crypto_ctx_ops_t skein_ctx_ops = {
	.create_ctx_template = skein_create_ctx_template,
	.free_context = skein_free_context
};
static const crypto_ops_t skein_crypto_ops = {
	&skein_digest_ops,
	NULL,
	&skein_mac_ops,
	&skein_ctx_ops,
};
static const crypto_provider_info_t skein_prov_info = {
	"Skein Software Provider",
	&skein_crypto_ops,
	sizeof (skein_mech_info_tab) / sizeof (crypto_mech_info_t),
	skein_mech_info_tab
};
static crypto_kcf_provider_handle_t skein_prov_handle = 0;
typedef struct skein_ctx {
	skein_mech_type_t		sc_mech_type;
	size_t				sc_digest_bitlen;
	union {
		Skein_256_Ctxt_t	sc_256;
		Skein_512_Ctxt_t	sc_512;
		Skein1024_Ctxt_t	sc_1024;
	};
} skein_ctx_t;
#define	SKEIN_CTX(_ctx_)	((skein_ctx_t *)((_ctx_)->cc_provider_private))
#define	SKEIN_CTX_LVALUE(_ctx_)	(_ctx_)->cc_provider_private
#define	SKEIN_OP(_skein_ctx, _op, ...)					\
	do {								\
		skein_ctx_t	*sc = (_skein_ctx);			\
		switch (sc->sc_mech_type) {				\
		case SKEIN_256_MECH_INFO_TYPE:				\
		case SKEIN_256_MAC_MECH_INFO_TYPE:			\
			(void) Skein_256_ ## _op(&sc->sc_256, __VA_ARGS__);\
			break;						\
		case SKEIN_512_MECH_INFO_TYPE:				\
		case SKEIN_512_MAC_MECH_INFO_TYPE:			\
			(void) Skein_512_ ## _op(&sc->sc_512, __VA_ARGS__);\
			break;						\
		case SKEIN1024_MECH_INFO_TYPE:				\
		case SKEIN1024_MAC_MECH_INFO_TYPE:			\
			(void) Skein1024_ ## _op(&sc->sc_1024, __VA_ARGS__);\
			break;						\
		}							\
	} while (0)
static int
skein_get_digest_bitlen(const crypto_mechanism_t *mechanism, size_t *result)
{
	if (mechanism->cm_param != NULL) {
		skein_param_t	*param = (skein_param_t *)mechanism->cm_param;
		if (mechanism->cm_param_len != sizeof (*param) ||
		    param->sp_digest_bitlen == 0) {
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		}
		*result = param->sp_digest_bitlen;
	} else {
		switch (mechanism->cm_type) {
		case SKEIN_256_MECH_INFO_TYPE:
			*result = 256;
			break;
		case SKEIN_512_MECH_INFO_TYPE:
			*result = 512;
			break;
		case SKEIN1024_MECH_INFO_TYPE:
			*result = 1024;
			break;
		default:
			return (CRYPTO_MECHANISM_INVALID);
		}
	}
	return (CRYPTO_SUCCESS);
}
int
skein_mod_init(void)
{
	(void) crypto_register_provider(&skein_prov_info, &skein_prov_handle);
	return (0);
}
int
skein_mod_fini(void)
{
	int ret = 0;
	if (skein_prov_handle != 0) {
		if ((ret = crypto_unregister_provider(skein_prov_handle)) !=
		    CRYPTO_SUCCESS) {
			cmn_err(CE_WARN,
			    "skein _fini: crypto_unregister_provider() "
			    "failed (0x%x)", ret);
			return (EBUSY);
		}
		skein_prov_handle = 0;
	}
	return (0);
}
static int
skein_digest_update_uio(skein_ctx_t *ctx, const crypto_data_t *data)
{
	off_t		offset = data->cd_offset;
	size_t		length = data->cd_length;
	uint_t		vec_idx = 0;
	size_t		cur_len;
	zfs_uio_t	*uio = data->cd_uio;
	if (zfs_uio_segflg(uio) != UIO_SYSSPACE)
		return (CRYPTO_ARGUMENTS_BAD);
	offset = zfs_uio_index_at_offset(uio, offset, &vec_idx);
	if (vec_idx == zfs_uio_iovcnt(uio)) {
		return (CRYPTO_DATA_LEN_RANGE);
	}
	while (vec_idx < zfs_uio_iovcnt(uio) && length > 0) {
		cur_len = MIN(zfs_uio_iovlen(uio, vec_idx) - offset, length);
		SKEIN_OP(ctx, Update, (uint8_t *)zfs_uio_iovbase(uio, vec_idx)
		    + offset, cur_len);
		length -= cur_len;
		vec_idx++;
		offset = 0;
	}
	if (vec_idx == zfs_uio_iovcnt(uio) && length > 0) {
		return (CRYPTO_DATA_LEN_RANGE);
	}
	return (CRYPTO_SUCCESS);
}
static int
skein_digest_final_uio(skein_ctx_t *ctx, crypto_data_t *digest)
{
	off_t offset = digest->cd_offset;
	uint_t vec_idx = 0;
	zfs_uio_t *uio = digest->cd_uio;
	if (zfs_uio_segflg(uio) != UIO_SYSSPACE)
		return (CRYPTO_ARGUMENTS_BAD);
	offset = zfs_uio_index_at_offset(uio, offset, &vec_idx);
	if (vec_idx == zfs_uio_iovcnt(uio)) {
		return (CRYPTO_DATA_LEN_RANGE);
	}
	if (offset + CRYPTO_BITS2BYTES(ctx->sc_digest_bitlen) <=
	    zfs_uio_iovlen(uio, vec_idx)) {
		SKEIN_OP(ctx, Final,
		    (uchar_t *)zfs_uio_iovbase(uio, vec_idx) + offset);
	} else {
		uint8_t *digest_tmp;
		off_t scratch_offset = 0;
		size_t length = CRYPTO_BITS2BYTES(ctx->sc_digest_bitlen);
		size_t cur_len;
		digest_tmp = kmem_alloc(CRYPTO_BITS2BYTES(
		    ctx->sc_digest_bitlen), KM_SLEEP);
		if (digest_tmp == NULL)
			return (CRYPTO_HOST_MEMORY);
		SKEIN_OP(ctx, Final, digest_tmp);
		while (vec_idx < zfs_uio_iovcnt(uio) && length > 0) {
			cur_len = MIN(zfs_uio_iovlen(uio, vec_idx) - offset,
			    length);
			memcpy(zfs_uio_iovbase(uio, vec_idx) + offset,
			    digest_tmp + scratch_offset, cur_len);
			length -= cur_len;
			vec_idx++;
			scratch_offset += cur_len;
			offset = 0;
		}
		kmem_free(digest_tmp, CRYPTO_BITS2BYTES(ctx->sc_digest_bitlen));
		if (vec_idx == zfs_uio_iovcnt(uio) && length > 0) {
			return (CRYPTO_DATA_LEN_RANGE);
		}
	}
	return (CRYPTO_SUCCESS);
}
static int
skein_digest_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism)
{
	int	error = CRYPTO_SUCCESS;
	if (!VALID_SKEIN_DIGEST_MECH(mechanism->cm_type))
		return (CRYPTO_MECHANISM_INVALID);
	SKEIN_CTX_LVALUE(ctx) = kmem_alloc(sizeof (*SKEIN_CTX(ctx)), KM_SLEEP);
	if (SKEIN_CTX(ctx) == NULL)
		return (CRYPTO_HOST_MEMORY);
	SKEIN_CTX(ctx)->sc_mech_type = mechanism->cm_type;
	error = skein_get_digest_bitlen(mechanism,
	    &SKEIN_CTX(ctx)->sc_digest_bitlen);
	if (error != CRYPTO_SUCCESS)
		goto errout;
	SKEIN_OP(SKEIN_CTX(ctx), Init, SKEIN_CTX(ctx)->sc_digest_bitlen);
	return (CRYPTO_SUCCESS);
errout:
	memset(SKEIN_CTX(ctx), 0, sizeof (*SKEIN_CTX(ctx)));
	kmem_free(SKEIN_CTX(ctx), sizeof (*SKEIN_CTX(ctx)));
	SKEIN_CTX_LVALUE(ctx) = NULL;
	return (error);
}
static int
skein_digest(crypto_ctx_t *ctx, crypto_data_t *data, crypto_data_t *digest)
{
	int error = CRYPTO_SUCCESS;
	ASSERT(SKEIN_CTX(ctx) != NULL);
	if (digest->cd_length <
	    CRYPTO_BITS2BYTES(SKEIN_CTX(ctx)->sc_digest_bitlen)) {
		digest->cd_length =
		    CRYPTO_BITS2BYTES(SKEIN_CTX(ctx)->sc_digest_bitlen);
		return (CRYPTO_BUFFER_TOO_SMALL);
	}
	error = skein_update(ctx, data);
	if (error != CRYPTO_SUCCESS) {
		memset(SKEIN_CTX(ctx), 0, sizeof (*SKEIN_CTX(ctx)));
		kmem_free(SKEIN_CTX(ctx), sizeof (*SKEIN_CTX(ctx)));
		SKEIN_CTX_LVALUE(ctx) = NULL;
		digest->cd_length = 0;
		return (error);
	}
	error = skein_final(ctx, digest);
	return (error);
}
static int
skein_update(crypto_ctx_t *ctx, crypto_data_t *data)
{
	int error = CRYPTO_SUCCESS;
	ASSERT(SKEIN_CTX(ctx) != NULL);
	switch (data->cd_format) {
	case CRYPTO_DATA_RAW:
		SKEIN_OP(SKEIN_CTX(ctx), Update,
		    (uint8_t *)data->cd_raw.iov_base + data->cd_offset,
		    data->cd_length);
		break;
	case CRYPTO_DATA_UIO:
		error = skein_digest_update_uio(SKEIN_CTX(ctx), data);
		break;
	default:
		error = CRYPTO_ARGUMENTS_BAD;
	}
	return (error);
}
static int
skein_final_nofree(crypto_ctx_t *ctx, crypto_data_t *digest)
{
	int error = CRYPTO_SUCCESS;
	ASSERT(SKEIN_CTX(ctx) != NULL);
	if (digest->cd_length <
	    CRYPTO_BITS2BYTES(SKEIN_CTX(ctx)->sc_digest_bitlen)) {
		digest->cd_length =
		    CRYPTO_BITS2BYTES(SKEIN_CTX(ctx)->sc_digest_bitlen);
		return (CRYPTO_BUFFER_TOO_SMALL);
	}
	switch (digest->cd_format) {
	case CRYPTO_DATA_RAW:
		SKEIN_OP(SKEIN_CTX(ctx), Final,
		    (uint8_t *)digest->cd_raw.iov_base + digest->cd_offset);
		break;
	case CRYPTO_DATA_UIO:
		error = skein_digest_final_uio(SKEIN_CTX(ctx), digest);
		break;
	default:
		error = CRYPTO_ARGUMENTS_BAD;
	}
	if (error == CRYPTO_SUCCESS)
		digest->cd_length =
		    CRYPTO_BITS2BYTES(SKEIN_CTX(ctx)->sc_digest_bitlen);
	else
		digest->cd_length = 0;
	return (error);
}
static int
skein_final(crypto_ctx_t *ctx, crypto_data_t *digest)
{
	int error = skein_final_nofree(ctx, digest);
	if (error == CRYPTO_BUFFER_TOO_SMALL)
		return (error);
	memset(SKEIN_CTX(ctx), 0, sizeof (*SKEIN_CTX(ctx)));
	kmem_free(SKEIN_CTX(ctx), sizeof (*(SKEIN_CTX(ctx))));
	SKEIN_CTX_LVALUE(ctx) = NULL;
	return (error);
}
static int
skein_digest_atomic(crypto_mechanism_t *mechanism, crypto_data_t *data,
    crypto_data_t *digest)
{
	int	 error;
	skein_ctx_t skein_ctx;
	crypto_ctx_t ctx;
	SKEIN_CTX_LVALUE(&ctx) = &skein_ctx;
	if (!VALID_SKEIN_DIGEST_MECH(mechanism->cm_type))
		return (CRYPTO_MECHANISM_INVALID);
	skein_ctx.sc_mech_type = mechanism->cm_type;
	error = skein_get_digest_bitlen(mechanism, &skein_ctx.sc_digest_bitlen);
	if (error != CRYPTO_SUCCESS)
		goto out;
	SKEIN_OP(&skein_ctx, Init, skein_ctx.sc_digest_bitlen);
	if ((error = skein_update(&ctx, data)) != CRYPTO_SUCCESS)
		goto out;
	if ((error = skein_final_nofree(&ctx, data)) != CRYPTO_SUCCESS)
		goto out;
out:
	if (error == CRYPTO_SUCCESS)
		digest->cd_length =
		    CRYPTO_BITS2BYTES(skein_ctx.sc_digest_bitlen);
	else
		digest->cd_length = 0;
	memset(&skein_ctx, 0, sizeof (skein_ctx));
	return (error);
}
static int
skein_mac_ctx_build(skein_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key)
{
	int error;
	if (!VALID_SKEIN_MAC_MECH(mechanism->cm_type))
		return (CRYPTO_MECHANISM_INVALID);
	ctx->sc_mech_type = mechanism->cm_type;
	error = skein_get_digest_bitlen(mechanism, &ctx->sc_digest_bitlen);
	if (error != CRYPTO_SUCCESS)
		return (error);
	SKEIN_OP(ctx, InitExt, ctx->sc_digest_bitlen, 0, key->ck_data,
	    CRYPTO_BITS2BYTES(key->ck_length));
	return (CRYPTO_SUCCESS);
}
static int
skein_mac_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t ctx_template)
{
	int	error;
	SKEIN_CTX_LVALUE(ctx) = kmem_alloc(sizeof (*SKEIN_CTX(ctx)), KM_SLEEP);
	if (SKEIN_CTX(ctx) == NULL)
		return (CRYPTO_HOST_MEMORY);
	if (ctx_template != NULL) {
		memcpy(SKEIN_CTX(ctx), ctx_template,
		    sizeof (*SKEIN_CTX(ctx)));
	} else {
		error = skein_mac_ctx_build(SKEIN_CTX(ctx), mechanism, key);
		if (error != CRYPTO_SUCCESS)
			goto errout;
	}
	return (CRYPTO_SUCCESS);
errout:
	memset(SKEIN_CTX(ctx), 0, sizeof (*SKEIN_CTX(ctx)));
	kmem_free(SKEIN_CTX(ctx), sizeof (*SKEIN_CTX(ctx)));
	return (error);
}
static int
skein_mac_atomic(crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *data, crypto_data_t *mac,
    crypto_spi_ctx_template_t ctx_template)
{
	int	error;
	crypto_ctx_t ctx;
	skein_ctx_t skein_ctx;
	SKEIN_CTX_LVALUE(&ctx) = &skein_ctx;
	if (ctx_template != NULL) {
		memcpy(&skein_ctx, ctx_template, sizeof (skein_ctx));
	} else {
		error = skein_mac_ctx_build(&skein_ctx, mechanism, key);
		if (error != CRYPTO_SUCCESS)
			goto errout;
	}
	if ((error = skein_update(&ctx, data)) != CRYPTO_SUCCESS)
		goto errout;
	if ((error = skein_final_nofree(&ctx, mac)) != CRYPTO_SUCCESS)
		goto errout;
	return (CRYPTO_SUCCESS);
errout:
	memset(&skein_ctx, 0, sizeof (skein_ctx));
	return (error);
}
static int
skein_create_ctx_template(crypto_mechanism_t *mechanism, crypto_key_t *key,
    crypto_spi_ctx_template_t *ctx_template, size_t *ctx_template_size)
{
	int	 error;
	skein_ctx_t *ctx_tmpl;
	ctx_tmpl = kmem_alloc(sizeof (*ctx_tmpl), KM_SLEEP);
	if (ctx_tmpl == NULL)
		return (CRYPTO_HOST_MEMORY);
	error = skein_mac_ctx_build(ctx_tmpl, mechanism, key);
	if (error != CRYPTO_SUCCESS)
		goto errout;
	*ctx_template = ctx_tmpl;
	*ctx_template_size = sizeof (*ctx_tmpl);
	return (CRYPTO_SUCCESS);
errout:
	memset(ctx_tmpl, 0, sizeof (*ctx_tmpl));
	kmem_free(ctx_tmpl, sizeof (*ctx_tmpl));
	return (error);
}
static int
skein_free_context(crypto_ctx_t *ctx)
{
	if (SKEIN_CTX(ctx) != NULL) {
		memset(SKEIN_CTX(ctx), 0, sizeof (*SKEIN_CTX(ctx)));
		kmem_free(SKEIN_CTX(ctx), sizeof (*SKEIN_CTX(ctx)));
		SKEIN_CTX_LVALUE(ctx) = NULL;
	}
	return (CRYPTO_SUCCESS);
}
