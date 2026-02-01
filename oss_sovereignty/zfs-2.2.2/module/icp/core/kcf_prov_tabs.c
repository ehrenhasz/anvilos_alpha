 
 

 

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/sched_impl.h>
#include <sys/crypto/spi.h>

#define	KCF_MAX_PROVIDERS	8	 

 
static kcf_provider_desc_t *prov_tab[KCF_MAX_PROVIDERS];
static kmutex_t prov_tab_mutex;  
static uint_t prov_tab_num = 0;  

void
kcf_prov_tab_destroy(void)
{
	mutex_destroy(&prov_tab_mutex);
}

 
void
kcf_prov_tab_init(void)
{
	mutex_init(&prov_tab_mutex, NULL, MUTEX_DEFAULT, NULL);
}

 
int
kcf_prov_tab_add_provider(kcf_provider_desc_t *prov_desc)
{
	uint_t i;

	mutex_enter(&prov_tab_mutex);

	 
	for (i = 1; i < KCF_MAX_PROVIDERS && prov_tab[i] != NULL; i++)
		;
	if (i == KCF_MAX_PROVIDERS) {
		 
		mutex_exit(&prov_tab_mutex);
		cmn_err(CE_WARN, "out of providers entries");
		return (CRYPTO_HOST_MEMORY);
	}

	 
	prov_tab[i] = prov_desc;
	KCF_PROV_REFHOLD(prov_desc);
	KCF_PROV_IREFHOLD(prov_desc);
	prov_tab_num++;

	mutex_exit(&prov_tab_mutex);

	 
	prov_desc->pd_prov_id = i;

	 
	prov_desc->pd_kcf_prov_handle =
	    (crypto_kcf_provider_handle_t)prov_desc->pd_prov_id;

	return (CRYPTO_SUCCESS);
}

 
int
kcf_prov_tab_rem_provider(crypto_provider_id_t prov_id)
{
	kcf_provider_desc_t *prov_desc;

	 

	mutex_enter(&prov_tab_mutex);
	if (prov_id >= KCF_MAX_PROVIDERS ||
	    ((prov_desc = prov_tab[prov_id]) == NULL)) {
		mutex_exit(&prov_tab_mutex);
		return (CRYPTO_INVALID_PROVIDER_ID);
	}
	mutex_exit(&prov_tab_mutex);

	 

	KCF_PROV_IREFRELE(prov_desc);
	KCF_PROV_REFRELE(prov_desc);

	return (CRYPTO_SUCCESS);
}

 
kcf_provider_desc_t *
kcf_prov_tab_lookup(crypto_provider_id_t prov_id)
{
	kcf_provider_desc_t *prov_desc;

	mutex_enter(&prov_tab_mutex);

	prov_desc = prov_tab[prov_id];

	if (prov_desc == NULL) {
		mutex_exit(&prov_tab_mutex);
		return (NULL);
	}

	KCF_PROV_REFHOLD(prov_desc);

	mutex_exit(&prov_tab_mutex);

	return (prov_desc);
}

 
kcf_provider_desc_t *
kcf_alloc_provider_desc(void)
{
	kcf_provider_desc_t *desc =
	    kmem_zalloc(sizeof (kcf_provider_desc_t), KM_SLEEP);

	for (int i = 0; i < KCF_OPS_CLASSSIZE; i++)
		for (int j = 0; j < KCF_MAXMECHTAB; j++)
			desc->pd_mech_indx[i][j] = KCF_INVALID_INDX;

	desc->pd_prov_id = KCF_PROVID_INVALID;
	desc->pd_state = KCF_PROV_ALLOCATED;

	mutex_init(&desc->pd_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&desc->pd_remove_cv, NULL, CV_DEFAULT, NULL);

	return (desc);
}

 
void
kcf_provider_zero_refcnt(kcf_provider_desc_t *desc)
{
	mutex_enter(&desc->pd_lock);
	if (desc->pd_state == KCF_PROV_REMOVED ||
	    desc->pd_state == KCF_PROV_DISABLED) {
		desc->pd_state = KCF_PROV_FREED;
		cv_broadcast(&desc->pd_remove_cv);
		mutex_exit(&desc->pd_lock);
		return;
	}

	mutex_exit(&desc->pd_lock);
	kcf_free_provider_desc(desc);
}

 
void
kcf_free_provider_desc(kcf_provider_desc_t *desc)
{
	if (desc == NULL)
		return;

	mutex_enter(&prov_tab_mutex);
	if (desc->pd_prov_id != KCF_PROVID_INVALID) {
		 
		ASSERT(prov_tab[desc->pd_prov_id] != NULL);
		prov_tab[desc->pd_prov_id] = NULL;
		prov_tab_num--;
	}
	mutex_exit(&prov_tab_mutex);

	 

	mutex_destroy(&desc->pd_lock);
	cv_destroy(&desc->pd_remove_cv);

	kmem_free(desc, sizeof (kcf_provider_desc_t));
}

 
int
kcf_get_sw_prov(crypto_mech_type_t mech_type, kcf_provider_desc_t **pd,
    kcf_mech_entry_t **mep, boolean_t log_warn)
{
	kcf_mech_entry_t *me;

	 
	if (kcf_get_mech_entry(mech_type, &me) != KCF_SUCCESS)
		return (CRYPTO_MECHANISM_INVALID);

	 
	if (me->me_sw_prov == NULL ||
	    (*pd = me->me_sw_prov->pm_prov_desc) == NULL) {
		 
		if (log_warn)
			cmn_err(CE_WARN, "no provider for \"%s\"\n",
			    me->me_name);
		return (CRYPTO_MECH_NOT_SUPPORTED);
	}

	KCF_PROV_REFHOLD(*pd);

	if (mep != NULL)
		*mep = me;

	return (CRYPTO_SUCCESS);
}
