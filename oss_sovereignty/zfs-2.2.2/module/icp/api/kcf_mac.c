 
 

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/api.h>
#include <sys/crypto/spi.h>
#include <sys/crypto/sched_impl.h>

 

 

 
int
crypto_mac(crypto_mechanism_t *mech, crypto_data_t *data,
    crypto_key_t *key, crypto_ctx_template_t tmpl, crypto_data_t *mac)
{
	int error;
	kcf_mech_entry_t *me;
	kcf_provider_desc_t *pd;
	kcf_ctx_template_t *ctx_tmpl;
	crypto_spi_ctx_template_t spi_ctx_tmpl = NULL;
	kcf_prov_tried_t *list = NULL;

retry:
	 
	if ((pd = kcf_get_mech_provider(mech->cm_type, &me, &error,
	    list, CRYPTO_FG_MAC_ATOMIC)) == NULL) {
		if (list != NULL)
			kcf_free_triedlist(list);
		return (error);
	}

	if (((ctx_tmpl = (kcf_ctx_template_t *)tmpl) != NULL))
		spi_ctx_tmpl = ctx_tmpl->ct_prov_tmpl;

	crypto_mechanism_t lmech = *mech;
	KCF_SET_PROVIDER_MECHNUM(mech->cm_type, pd, &lmech);
	error = KCF_PROV_MAC_ATOMIC(pd, &lmech, key, data,
	    mac, spi_ctx_tmpl);

	if (error != CRYPTO_SUCCESS && IS_RECOVERABLE(error)) {
		 
		if (kcf_insert_triedlist(&list, pd, KM_SLEEP) != NULL)
			goto retry;
	}

	if (list != NULL)
		kcf_free_triedlist(list);

	KCF_PROV_REFRELE(pd);
	return (error);
}

 
static int
crypto_mac_init_prov(kcf_provider_desc_t *pd,
    crypto_mechanism_t *mech, crypto_key_t *key, crypto_spi_ctx_template_t tmpl,
    crypto_context_t *ctxp)
{
	int rv;
	crypto_ctx_t *ctx;
	kcf_provider_desc_t *real_provider = pd;

	ASSERT(KCF_PROV_REFHELD(pd));

	 
	if ((ctx = kcf_new_ctx(real_provider)) == NULL)
		return (CRYPTO_HOST_MEMORY);

	crypto_mechanism_t lmech = *mech;
	KCF_SET_PROVIDER_MECHNUM(mech->cm_type, real_provider, &lmech);
	rv = KCF_PROV_MAC_INIT(real_provider, ctx, &lmech, key, tmpl);

	if (rv == CRYPTO_SUCCESS)
		*ctxp = (crypto_context_t)ctx;
	else {
		 
		KCF_CONTEXT_REFRELE((kcf_context_t *)ctx->cc_framework_private);
	}

	return (rv);
}

 
int
crypto_mac_init(crypto_mechanism_t *mech, crypto_key_t *key,
    crypto_ctx_template_t tmpl, crypto_context_t *ctxp)
{
	int error;
	kcf_mech_entry_t *me;
	kcf_provider_desc_t *pd;
	kcf_ctx_template_t *ctx_tmpl;
	crypto_spi_ctx_template_t spi_ctx_tmpl = NULL;
	kcf_prov_tried_t *list = NULL;

retry:
	 
	if ((pd = kcf_get_mech_provider(mech->cm_type, &me, &error,
	    list, CRYPTO_FG_MAC)) == NULL) {
		if (list != NULL)
			kcf_free_triedlist(list);
		return (error);
	}

	 

	if (((ctx_tmpl = (kcf_ctx_template_t *)tmpl) != NULL))
		spi_ctx_tmpl = ctx_tmpl->ct_prov_tmpl;

	error = crypto_mac_init_prov(pd, mech, key,
	    spi_ctx_tmpl, ctxp);
	if (error != CRYPTO_SUCCESS && IS_RECOVERABLE(error)) {
		 
		if (kcf_insert_triedlist(&list, pd, KM_SLEEP) != NULL)
			goto retry;
	}

	if (list != NULL)
		kcf_free_triedlist(list);

	KCF_PROV_REFRELE(pd);
	return (error);
}

 
int
crypto_mac_update(crypto_context_t context, crypto_data_t *data)
{
	crypto_ctx_t *ctx = (crypto_ctx_t *)context;
	kcf_context_t *kcf_ctx;
	kcf_provider_desc_t *pd;

	if ((ctx == NULL) ||
	    ((kcf_ctx = (kcf_context_t *)ctx->cc_framework_private) == NULL) ||
	    ((pd = kcf_ctx->kc_prov_desc) == NULL)) {
		return (CRYPTO_INVALID_CONTEXT);
	}

	return (KCF_PROV_MAC_UPDATE(pd, ctx, data));
}

 
int
crypto_mac_final(crypto_context_t context, crypto_data_t *mac)
{
	crypto_ctx_t *ctx = (crypto_ctx_t *)context;
	kcf_context_t *kcf_ctx;
	kcf_provider_desc_t *pd;

	if ((ctx == NULL) ||
	    ((kcf_ctx = (kcf_context_t *)ctx->cc_framework_private) == NULL) ||
	    ((pd = kcf_ctx->kc_prov_desc) == NULL)) {
		return (CRYPTO_INVALID_CONTEXT);
	}

	int rv = KCF_PROV_MAC_FINAL(pd, ctx, mac);

	 
	KCF_CONTEXT_COND_RELEASE(rv, kcf_ctx);
	return (rv);
}

#if defined(_KERNEL)
EXPORT_SYMBOL(crypto_mac);
EXPORT_SYMBOL(crypto_mac_init);
EXPORT_SYMBOL(crypto_mac_update);
EXPORT_SYMBOL(crypto_mac_final);
#endif
