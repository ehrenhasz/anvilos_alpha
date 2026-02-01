 
 

 


#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/sched_impl.h>
#include <sys/crypto/spi.h>

static int init_prov_mechs(const crypto_provider_info_t *,
    kcf_provider_desc_t *);

 
int
crypto_register_provider(const crypto_provider_info_t *info,
    crypto_kcf_provider_handle_t *handle)
{
	kcf_provider_desc_t *prov_desc = NULL;
	int ret = CRYPTO_ARGUMENTS_BAD;

	 
	prov_desc = kcf_alloc_provider_desc();
	KCF_PROV_REFHOLD(prov_desc);

	 
	prov_desc->pd_description = info->pi_provider_description;

	 
	prov_desc->pd_ops_vector = info->pi_ops_vector;

	 
	if ((ret = init_prov_mechs(info, prov_desc)) != CRYPTO_SUCCESS)
		goto bail;

	 
	if ((ret = kcf_prov_tab_add_provider(prov_desc)) != CRYPTO_SUCCESS) {
		undo_register_provider(prov_desc, B_FALSE);
		goto bail;
	}

	 

	mutex_enter(&prov_desc->pd_lock);
	prov_desc->pd_state = KCF_PROV_READY;
	mutex_exit(&prov_desc->pd_lock);

	*handle = prov_desc->pd_kcf_prov_handle;
	ret = CRYPTO_SUCCESS;

bail:
	KCF_PROV_REFRELE(prov_desc);
	return (ret);
}

 
int
crypto_unregister_provider(crypto_kcf_provider_handle_t handle)
{
	uint_t mech_idx;
	kcf_provider_desc_t *desc;
	kcf_prov_state_t saved_state;

	 
	if ((desc = kcf_prov_tab_lookup((crypto_provider_id_t)handle)) == NULL)
		return (CRYPTO_UNKNOWN_PROVIDER);

	mutex_enter(&desc->pd_lock);
	 
	if (desc->pd_state >= KCF_PROV_DISABLED) {
		mutex_exit(&desc->pd_lock);
		 
		KCF_PROV_REFRELE(desc);
		return (CRYPTO_BUSY);
	}

	saved_state = desc->pd_state;
	desc->pd_state = KCF_PROV_REMOVED;

	 
	if (desc->pd_refcnt > desc->pd_irefcnt + 1) {
		desc->pd_state = saved_state;
		mutex_exit(&desc->pd_lock);
		 
		KCF_PROV_REFRELE(desc);
		 
		return (CRYPTO_BUSY);
	}
	mutex_exit(&desc->pd_lock);

	 
	for (mech_idx = 0; mech_idx < desc->pd_mech_list_count;
	    mech_idx++) {
		kcf_remove_mech_provider(
		    desc->pd_mechanisms[mech_idx].cm_mech_name, desc);
	}

	 
	if (kcf_prov_tab_rem_provider((crypto_provider_id_t)handle) !=
	    CRYPTO_SUCCESS) {
		 
		KCF_PROV_REFRELE(desc);
		return (CRYPTO_UNKNOWN_PROVIDER);
	}

	 
	KCF_PROV_REFRELE(desc);

	 
	mutex_enter(&desc->pd_lock);
	while (desc->pd_state != KCF_PROV_FREED)
		cv_wait(&desc->pd_remove_cv, &desc->pd_lock);
	mutex_exit(&desc->pd_lock);

	 
	ASSERT(desc->pd_state == KCF_PROV_FREED &&
	    desc->pd_refcnt == 0);
	kcf_free_provider_desc(desc);

	return (CRYPTO_SUCCESS);
}

 
static int
init_prov_mechs(const crypto_provider_info_t *info, kcf_provider_desc_t *desc)
{
	uint_t mech_idx;
	uint_t cleanup_idx;
	int err = CRYPTO_SUCCESS;
	kcf_prov_mech_desc_t *pmd;
	int desc_use_count = 0;

	 
	if (info != NULL) {
		ASSERT(info->pi_mechanisms != NULL);
		desc->pd_mech_list_count = info->pi_mech_list_count;
		desc->pd_mechanisms = info->pi_mechanisms;
	}

	 
	for (mech_idx = 0; mech_idx < desc->pd_mech_list_count; mech_idx++) {
		if ((err = kcf_add_mech_provider(mech_idx, desc, &pmd)) !=
		    KCF_SUCCESS)
			break;

		if (pmd == NULL)
			continue;

		 
		desc_use_count++;
	}

	 
	if (desc_use_count == 0)
		return (CRYPTO_ARGUMENTS_BAD);

	if (err == KCF_SUCCESS)
		return (CRYPTO_SUCCESS);

	 
	for (cleanup_idx = 0; cleanup_idx < mech_idx; cleanup_idx++) {
		kcf_remove_mech_provider(
		    desc->pd_mechanisms[cleanup_idx].cm_mech_name, desc);
	}

	if (err == KCF_MECH_TAB_FULL)
		return (CRYPTO_HOST_MEMORY);

	return (CRYPTO_ARGUMENTS_BAD);
}

 
void
undo_register_provider(kcf_provider_desc_t *desc, boolean_t remove_prov)
{
	uint_t mech_idx;

	 
	for (mech_idx = 0; mech_idx < desc->pd_mech_list_count;
	    mech_idx++) {
		kcf_remove_mech_provider(
		    desc->pd_mechanisms[mech_idx].cm_mech_name, desc);
	}

	 
	if (remove_prov)
		(void) kcf_prov_tab_rem_provider(desc->pd_prov_id);
}
