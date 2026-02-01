 
 

 

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/sched_impl.h>
#include <sys/crypto/api.h>

 
static kmem_cache_t *kcf_context_cache;

 
crypto_ctx_t *
kcf_new_ctx(kcf_provider_desc_t *pd)
{
	crypto_ctx_t *ctx;
	kcf_context_t *kcf_ctx;

	kcf_ctx = kmem_cache_alloc(kcf_context_cache, KM_SLEEP);
	if (kcf_ctx == NULL)
		return (NULL);

	 
	kcf_ctx->kc_refcnt = 1;
	KCF_PROV_REFHOLD(pd);
	kcf_ctx->kc_prov_desc = pd;
	kcf_ctx->kc_sw_prov_desc = NULL;

	ctx = &kcf_ctx->kc_glbl_ctx;
	ctx->cc_provider_private = NULL;
	ctx->cc_framework_private = (void *)kcf_ctx;

	return (ctx);
}

 
void
kcf_free_context(kcf_context_t *kcf_ctx)
{
	kcf_provider_desc_t *pd = kcf_ctx->kc_prov_desc;
	crypto_ctx_t *gctx = &kcf_ctx->kc_glbl_ctx;

	if (gctx->cc_provider_private != NULL) {
		mutex_enter(&pd->pd_lock);
		if (!KCF_IS_PROV_REMOVED(pd)) {
			 
			KCF_PROV_IREFHOLD(pd);
			mutex_exit(&pd->pd_lock);
			(void) KCF_PROV_FREE_CONTEXT(pd, gctx);
			KCF_PROV_IREFRELE(pd);
		} else {
			mutex_exit(&pd->pd_lock);
		}
	}

	 
	KCF_PROV_REFRELE(kcf_ctx->kc_prov_desc);

	kmem_cache_free(kcf_context_cache, kcf_ctx);
}

 
static int
kcf_context_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	(void) cdrarg, (void) kmflags;
	kcf_context_t *kctx = (kcf_context_t *)buf;

	kctx->kc_refcnt = 0;

	return (0);
}

static void
kcf_context_cache_destructor(void *buf, void *cdrarg)
{
	(void) cdrarg;
	kcf_context_t *kctx = (kcf_context_t *)buf;

	ASSERT(kctx->kc_refcnt == 0);
}

void
kcf_sched_destroy(void)
{
	if (kcf_context_cache)
		kmem_cache_destroy(kcf_context_cache);
}

 
void
kcf_sched_init(void)
{
	 
	kcf_context_cache = kmem_cache_create("kcf_context_cache",
	    sizeof (struct kcf_context), 64, kcf_context_cache_constructor,
	    kcf_context_cache_destructor, NULL, NULL, NULL, 0);
}
