 
 

#ifndef _SYS_CRYPTO_SCHED_IMPL_H
#define	_SYS_CRYPTO_SCHED_IMPL_H

 

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/zfs_context.h>
#include <sys/crypto/api.h>
#include <sys/crypto/spi.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/common.h>

typedef struct kcf_prov_tried {
	kcf_provider_desc_t	*pt_pd;
	struct kcf_prov_tried	*pt_next;
} kcf_prov_tried_t;

#define	IS_FG_SUPPORTED(mdesc, fg)		\
	(((mdesc)->pm_mech_info.cm_func_group_mask & (fg)) != 0)

#define	IS_PROVIDER_TRIED(pd, tlist)		\
	(tlist != NULL && is_in_triedlist(pd, tlist))

#define	IS_RECOVERABLE(error)			\
	(error == CRYPTO_BUSY ||			\
	error == CRYPTO_KEY_SIZE_RANGE)

 
typedef struct kcf_context {
	crypto_ctx_t		kc_glbl_ctx;
	uint_t			kc_refcnt;
	kcf_provider_desc_t	*kc_prov_desc;	 
	kcf_provider_desc_t	*kc_sw_prov_desc;	 
} kcf_context_t;

 
#define	KCF_CONTEXT_REFRELE(ictx) {				\
	membar_producer();					\
	int newval = atomic_add_32_nv(&(ictx)->kc_refcnt, -1);	\
	ASSERT(newval != -1);					\
	if (newval == 0)					\
		kcf_free_context(ictx);				\
}

 
#define	KCF_CONTEXT_COND_RELEASE(rv, kcf_ctx) {			\
	if (KCF_CONTEXT_DONE(rv))				\
		KCF_CONTEXT_REFRELE(kcf_ctx);			\
}

 
#define	KCF_CONTEXT_DONE(rv)					\
	((rv) != CRYPTO_BUSY &&	(rv) != CRYPTO_BUFFER_TOO_SMALL)


#define	KCF_SET_PROVIDER_MECHNUM(fmtype, pd, mechp)			\
	(mechp)->cm_type =						\
	    KCF_TO_PROV_MECHNUM(pd, fmtype);

 
typedef	struct kcf_ctx_template {
	size_t				ct_size;	 
	crypto_spi_ctx_template_t	ct_prov_tmpl;	 
							 
} kcf_ctx_template_t;


extern void kcf_free_triedlist(kcf_prov_tried_t *);
extern kcf_prov_tried_t *kcf_insert_triedlist(kcf_prov_tried_t **,
    kcf_provider_desc_t *, int);
extern kcf_provider_desc_t *kcf_get_mech_provider(crypto_mech_type_t,
    kcf_mech_entry_t **, int *, kcf_prov_tried_t *, crypto_func_group_t);
extern crypto_ctx_t *kcf_new_ctx(kcf_provider_desc_t *);
extern void kcf_sched_destroy(void);
extern void kcf_sched_init(void);
extern void kcf_free_context(kcf_context_t *);

#ifdef __cplusplus
}
#endif

#endif  
