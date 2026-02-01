
 

#include <kunit/test.h>
#include <kunit/static_stub.h>
#include "hooks-impl.h"


 
struct kunit_static_stub_ctx {
	void *real_fn_addr;
	void *replacement_addr;
};

static void __kunit_static_stub_resource_free(struct kunit_resource *res)
{
	kfree(res->data);
}

 
static bool __kunit_static_stub_resource_match(struct kunit *test,
						struct kunit_resource *res,
						void *match_real_fn_addr)
{
	 
	struct kunit_static_stub_ctx *ctx = res->data;

	 
	if (res->free != &__kunit_static_stub_resource_free)
		return false;

	return ctx->real_fn_addr == match_real_fn_addr;
}

 
void *__kunit_get_static_stub_address_impl(struct kunit *test, void *real_fn_addr)
{
	struct kunit_resource *res;
	struct kunit_static_stub_ctx *ctx;
	void *replacement_addr;

	res = kunit_find_resource(test,
				  __kunit_static_stub_resource_match,
				  real_fn_addr);

	if (!res)
		return NULL;

	ctx = res->data;
	replacement_addr = ctx->replacement_addr;
	kunit_put_resource(res);
	return replacement_addr;
}

void kunit_deactivate_static_stub(struct kunit *test, void *real_fn_addr)
{
	struct kunit_resource *res;

	KUNIT_ASSERT_PTR_NE_MSG(test, real_fn_addr, NULL,
				"Tried to deactivate a NULL stub.");

	 
	res = kunit_find_resource(test,
				  __kunit_static_stub_resource_match,
				  real_fn_addr);

	 
	KUNIT_ASSERT_PTR_NE_MSG(test, res, NULL,
				"Tried to deactivate a nonexistent stub.");

	 
	kunit_remove_resource(test, res);
	kunit_put_resource(res);
}
EXPORT_SYMBOL_GPL(kunit_deactivate_static_stub);

 
void __kunit_activate_static_stub(struct kunit *test,
				  void *real_fn_addr,
				  void *replacement_addr)
{
	struct kunit_static_stub_ctx *ctx;
	struct kunit_resource *res;

	KUNIT_ASSERT_PTR_NE_MSG(test, real_fn_addr, NULL,
				"Tried to activate a stub for function NULL");

	 
	if (!replacement_addr) {
		kunit_deactivate_static_stub(test, replacement_addr);
		return;
	}

	 
	res = kunit_find_resource(test,
				  __kunit_static_stub_resource_match,
				  real_fn_addr);
	if (res) {
		ctx = res->data;
		ctx->replacement_addr = replacement_addr;

		 
		kunit_put_resource(res);
	} else {
		ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
		ctx->real_fn_addr = real_fn_addr;
		ctx->replacement_addr = replacement_addr;
		res = kunit_alloc_resource(test, NULL,
				     &__kunit_static_stub_resource_free,
				     GFP_KERNEL, ctx);
	}
}
EXPORT_SYMBOL_GPL(__kunit_activate_static_stub);
