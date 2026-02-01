 
 

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/api.h>
#include <sys/crypto/spi.h>
#include <sys/crypto/sched_impl.h>

 

 
int
crypto_create_ctx_template(crypto_mechanism_t *mech, crypto_key_t *key,
    crypto_ctx_template_t *ptmpl)
{
	int error;
	kcf_mech_entry_t *me;
	kcf_provider_desc_t *pd;
	kcf_ctx_template_t *ctx_tmpl;
	crypto_mechanism_t prov_mech;

	 

	if (ptmpl == NULL)
		return (CRYPTO_ARGUMENTS_BAD);

	if (mech == NULL)
		return (CRYPTO_MECHANISM_INVALID);

	error = kcf_get_sw_prov(mech->cm_type, &pd, &me, B_TRUE);
	if (error != CRYPTO_SUCCESS)
		return (error);

	if ((ctx_tmpl = kmem_alloc(
	    sizeof (kcf_ctx_template_t), KM_SLEEP)) == NULL) {
		KCF_PROV_REFRELE(pd);
		return (CRYPTO_HOST_MEMORY);
	}

	 
	prov_mech.cm_type = KCF_TO_PROV_MECHNUM(pd, mech->cm_type);
	prov_mech.cm_param = mech->cm_param;
	prov_mech.cm_param_len = mech->cm_param_len;

	error = KCF_PROV_CREATE_CTX_TEMPLATE(pd, &prov_mech, key,
	    &(ctx_tmpl->ct_prov_tmpl), &(ctx_tmpl->ct_size));

	if (error == CRYPTO_SUCCESS) {
		*ptmpl = ctx_tmpl;
	} else {
		kmem_free(ctx_tmpl, sizeof (kcf_ctx_template_t));
	}
	KCF_PROV_REFRELE(pd);

	return (error);
}

 
void
crypto_destroy_ctx_template(crypto_ctx_template_t tmpl)
{
	kcf_ctx_template_t *ctx_tmpl = (kcf_ctx_template_t *)tmpl;

	if (ctx_tmpl == NULL)
		return;

	ASSERT(ctx_tmpl->ct_prov_tmpl != NULL);

	memset(ctx_tmpl->ct_prov_tmpl, 0, ctx_tmpl->ct_size);
	kmem_free(ctx_tmpl->ct_prov_tmpl, ctx_tmpl->ct_size);
	kmem_free(ctx_tmpl, sizeof (kcf_ctx_template_t));
}

#if defined(_KERNEL)
EXPORT_SYMBOL(crypto_create_ctx_template);
EXPORT_SYMBOL(crypto_destroy_ctx_template);
#endif
