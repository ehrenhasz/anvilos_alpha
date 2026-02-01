 
 

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/api.h>
#include <sys/crypto/spi.h>
#include <sys/crypto/sched_impl.h>

 


 
int
crypto_encrypt(crypto_mechanism_t *mech, crypto_data_t *plaintext,
    crypto_key_t *key, crypto_ctx_template_t tmpl, crypto_data_t *ciphertext)
{
	int error;
	kcf_mech_entry_t *me;
	kcf_provider_desc_t *pd;
	kcf_ctx_template_t *ctx_tmpl;
	crypto_spi_ctx_template_t spi_ctx_tmpl = NULL;
	kcf_prov_tried_t *list = NULL;

retry:
	 
	if ((pd = kcf_get_mech_provider(mech->cm_type, &me, &error,
	    list, CRYPTO_FG_ENCRYPT_ATOMIC)) == NULL) {
		if (list != NULL)
			kcf_free_triedlist(list);
		return (error);
	}

	if (((ctx_tmpl = (kcf_ctx_template_t *)tmpl) != NULL))
		spi_ctx_tmpl = ctx_tmpl->ct_prov_tmpl;

	crypto_mechanism_t lmech = *mech;
	KCF_SET_PROVIDER_MECHNUM(mech->cm_type, pd, &lmech);
	error = KCF_PROV_ENCRYPT_ATOMIC(pd, &lmech, key,
	    plaintext, ciphertext, spi_ctx_tmpl);

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
crypto_decrypt(crypto_mechanism_t *mech, crypto_data_t *ciphertext,
    crypto_key_t *key, crypto_ctx_template_t tmpl, crypto_data_t *plaintext)
{
	int error;
	kcf_mech_entry_t *me;
	kcf_provider_desc_t *pd;
	kcf_ctx_template_t *ctx_tmpl;
	crypto_spi_ctx_template_t spi_ctx_tmpl = NULL;
	kcf_prov_tried_t *list = NULL;

retry:
	 
	if ((pd = kcf_get_mech_provider(mech->cm_type, &me, &error,
	    list, CRYPTO_FG_DECRYPT_ATOMIC)) == NULL) {
		if (list != NULL)
			kcf_free_triedlist(list);
		return (error);
	}

	if (((ctx_tmpl = (kcf_ctx_template_t *)tmpl) != NULL))
		spi_ctx_tmpl = ctx_tmpl->ct_prov_tmpl;

	crypto_mechanism_t lmech = *mech;
	KCF_SET_PROVIDER_MECHNUM(mech->cm_type, pd, &lmech);

	error = KCF_PROV_DECRYPT_ATOMIC(pd, &lmech, key,
	    ciphertext, plaintext, spi_ctx_tmpl);

	if (error != CRYPTO_SUCCESS && IS_RECOVERABLE(error)) {
		 
		if (kcf_insert_triedlist(&list, pd, KM_SLEEP) != NULL)
			goto retry;
	}

	if (list != NULL)
		kcf_free_triedlist(list);

	KCF_PROV_REFRELE(pd);
	return (error);
}

#if defined(_KERNEL)
EXPORT_SYMBOL(crypto_encrypt);
EXPORT_SYMBOL(crypto_decrypt);
#endif
