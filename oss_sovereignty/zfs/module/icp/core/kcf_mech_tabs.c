#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/api.h>
#include <sys/crypto/impl.h>
static kcf_mech_entry_t kcf_digest_mechs_tab[KCF_MAXDIGEST];
static kcf_mech_entry_t kcf_cipher_mechs_tab[KCF_MAXCIPHER];
static kcf_mech_entry_t kcf_mac_mechs_tab[KCF_MAXMAC];
const kcf_mech_entry_tab_t kcf_mech_tabs_tab[KCF_LAST_OPSCLASS + 1] = {
	{0, NULL},				 
	{KCF_MAXDIGEST, kcf_digest_mechs_tab},
	{KCF_MAXCIPHER, kcf_cipher_mechs_tab},
	{KCF_MAXMAC, kcf_mac_mechs_tab},
};
static avl_tree_t kcf_mech_hash;
static int
kcf_mech_hash_compar(const void *lhs, const void *rhs)
{
	const kcf_mech_entry_t *l = lhs, *r = rhs;
	int cmp = strncmp(l->me_name, r->me_name, CRYPTO_MAX_MECH_NAME);
	return ((0 < cmp) - (cmp < 0));
}
void
kcf_destroy_mech_tabs(void)
{
	for (void *cookie = NULL; avl_destroy_nodes(&kcf_mech_hash, &cookie); )
		;
	avl_destroy(&kcf_mech_hash);
}
void
kcf_init_mech_tabs(void)
{
	avl_create(&kcf_mech_hash, kcf_mech_hash_compar,
	    sizeof (kcf_mech_entry_t), offsetof(kcf_mech_entry_t, me_node));
}
static int
kcf_create_mech_entry(kcf_ops_class_t class, const char *mechname)
{
	if ((class < KCF_FIRST_OPSCLASS) || (class > KCF_LAST_OPSCLASS))
		return (KCF_INVALID_MECH_CLASS);
	if ((mechname == NULL) || (mechname[0] == 0))
		return (KCF_INVALID_MECH_NAME);
	avl_index_t where = 0;
	kcf_mech_entry_t tmptab;
	strlcpy(tmptab.me_name, mechname, CRYPTO_MAX_MECH_NAME);
	if (avl_find(&kcf_mech_hash, &tmptab, &where) != NULL)
		return (KCF_SUCCESS);
	kcf_mech_entry_t *me_tab = kcf_mech_tabs_tab[class].met_tab;
	int size = kcf_mech_tabs_tab[class].met_size;
	for (int i = 0; i < size; ++i)
		if (me_tab[i].me_name[0] == 0) {
			strlcpy(me_tab[i].me_name, mechname,
			    CRYPTO_MAX_MECH_NAME);
			me_tab[i].me_mechid = KCF_MECHID(class, i);
			avl_insert(&kcf_mech_hash, &me_tab[i], where);
			return (KCF_SUCCESS);
		}
	return (KCF_MECH_TAB_FULL);
}
int
kcf_add_mech_provider(short mech_indx,
    kcf_provider_desc_t *prov_desc, kcf_prov_mech_desc_t **pmdpp)
{
	int error;
	kcf_mech_entry_t *mech_entry = NULL;
	const crypto_mech_info_t *mech_info;
	crypto_mech_type_t kcf_mech_type;
	kcf_prov_mech_desc_t *prov_mech;
	mech_info = &prov_desc->pd_mechanisms[mech_indx];
	kcf_mech_type = crypto_mech2id(mech_info->cm_mech_name);
	if (kcf_mech_type == CRYPTO_MECH_INVALID) {
		crypto_func_group_t fg = mech_info->cm_func_group_mask;
		kcf_ops_class_t class;
		if (fg & CRYPTO_FG_DIGEST || fg & CRYPTO_FG_DIGEST_ATOMIC)
			class = KCF_DIGEST_CLASS;
		else if (fg & CRYPTO_FG_ENCRYPT || fg & CRYPTO_FG_DECRYPT ||
		    fg & CRYPTO_FG_ENCRYPT_ATOMIC ||
		    fg & CRYPTO_FG_DECRYPT_ATOMIC)
			class = KCF_CIPHER_CLASS;
		else if (fg & CRYPTO_FG_MAC || fg & CRYPTO_FG_MAC_ATOMIC)
			class = KCF_MAC_CLASS;
		else
			__builtin_unreachable();
		if ((error = kcf_create_mech_entry(class,
		    mech_info->cm_mech_name)) != KCF_SUCCESS) {
			return (error);
		}
		kcf_mech_type = crypto_mech2id(mech_info->cm_mech_name);
		ASSERT(kcf_mech_type != CRYPTO_MECH_INVALID);
	}
	error = kcf_get_mech_entry(kcf_mech_type, &mech_entry);
	ASSERT(error == KCF_SUCCESS);
	prov_mech = kmem_zalloc(sizeof (kcf_prov_mech_desc_t), KM_SLEEP);
	memcpy(&prov_mech->pm_mech_info, mech_info,
	    sizeof (crypto_mech_info_t));
	prov_mech->pm_prov_desc = prov_desc;
	prov_desc->pd_mech_indx[KCF_MECH2CLASS(kcf_mech_type)]
	    [KCF_MECH2INDEX(kcf_mech_type)] = mech_indx;
	KCF_PROV_REFHOLD(prov_desc);
	KCF_PROV_IREFHOLD(prov_desc);
	if (mech_entry->me_sw_prov != NULL) {
		cmn_err(CE_WARN, "The cryptographic provider "
		    "\"%s\" will not be used for %s. The provider "
		    "\"%s\" will be used for this mechanism "
		    "instead.", prov_desc->pd_description,
		    mech_info->cm_mech_name,
		    mech_entry->me_sw_prov->pm_prov_desc->
		    pd_description);
		KCF_PROV_REFRELE(prov_desc);
		kmem_free(prov_mech, sizeof (kcf_prov_mech_desc_t));
		prov_mech = NULL;
	} else {
		mech_entry->me_sw_prov = prov_mech;
	}
	*pmdpp = prov_mech;
	return (KCF_SUCCESS);
}
void
kcf_remove_mech_provider(const char *mech_name, kcf_provider_desc_t *prov_desc)
{
	crypto_mech_type_t mech_type;
	kcf_prov_mech_desc_t *prov_mech = NULL;
	kcf_mech_entry_t *mech_entry;
	if ((mech_type = crypto_mech2id(mech_name)) ==
	    CRYPTO_MECH_INVALID) {
		return;
	}
	if (kcf_get_mech_entry(mech_type, &mech_entry) != KCF_SUCCESS) {
		return;
	}
	if (mech_entry->me_sw_prov == NULL ||
	    mech_entry->me_sw_prov->pm_prov_desc != prov_desc) {
		return;
	}
	prov_mech = mech_entry->me_sw_prov;
	mech_entry->me_sw_prov = NULL;
	KCF_PROV_IREFRELE(prov_mech->pm_prov_desc);
	KCF_PROV_REFRELE(prov_mech->pm_prov_desc);
	kmem_free(prov_mech, sizeof (kcf_prov_mech_desc_t));
}
int
kcf_get_mech_entry(crypto_mech_type_t mech_type, kcf_mech_entry_t **mep)
{
	kcf_ops_class_t		class;
	int			index;
	const kcf_mech_entry_tab_t	*me_tab;
	ASSERT(mep != NULL);
	class = KCF_MECH2CLASS(mech_type);
	if ((class < KCF_FIRST_OPSCLASS) || (class > KCF_LAST_OPSCLASS)) {
		return (KCF_INVALID_MECH_NUMBER);
	}
	me_tab = &kcf_mech_tabs_tab[class];
	index = KCF_MECH2INDEX(mech_type);
	if ((index < 0) || (index >= me_tab->met_size)) {
		return (KCF_INVALID_MECH_NUMBER);
	}
	*mep = &((me_tab->met_tab)[index]);
	return (KCF_SUCCESS);
}
crypto_mech_type_t
crypto_mech2id(const char *mechname)
{
	kcf_mech_entry_t tmptab, *found;
	strlcpy(tmptab.me_name, mechname, CRYPTO_MAX_MECH_NAME);
	if ((found = avl_find(&kcf_mech_hash, &tmptab, NULL))) {
		ASSERT(found->me_mechid != CRYPTO_MECH_INVALID);
		return (found->me_mechid);
	}
	return (CRYPTO_MECH_INVALID);
}
#if defined(_KERNEL)
EXPORT_SYMBOL(crypto_mech2id);
#endif
