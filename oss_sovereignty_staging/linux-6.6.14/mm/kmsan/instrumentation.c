
 

#include "kmsan.h"
#include <linux/gfp.h>
#include <linux/kmsan_string.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

static inline bool is_bad_asm_addr(void *addr, uintptr_t size, bool is_store)
{
	if ((u64)addr < TASK_SIZE)
		return true;
	if (!kmsan_get_metadata(addr, KMSAN_META_SHADOW))
		return true;
	return false;
}

static inline struct shadow_origin_ptr
get_shadow_origin_ptr(void *addr, u64 size, bool store)
{
	unsigned long ua_flags = user_access_save();
	struct shadow_origin_ptr ret;

	ret = kmsan_get_shadow_origin_ptr(addr, size, store);
	user_access_restore(ua_flags);
	return ret;
}

 

 
struct shadow_origin_ptr __msan_metadata_ptr_for_load_n(void *addr,
							uintptr_t size);
struct shadow_origin_ptr __msan_metadata_ptr_for_load_n(void *addr,
							uintptr_t size)
{
	return get_shadow_origin_ptr(addr, size,   false);
}
EXPORT_SYMBOL(__msan_metadata_ptr_for_load_n);

 
struct shadow_origin_ptr __msan_metadata_ptr_for_store_n(void *addr,
							 uintptr_t size);
struct shadow_origin_ptr __msan_metadata_ptr_for_store_n(void *addr,
							 uintptr_t size)
{
	return get_shadow_origin_ptr(addr, size,   true);
}
EXPORT_SYMBOL(__msan_metadata_ptr_for_store_n);

 
#define DECLARE_METADATA_PTR_GETTER(size)                                  \
	struct shadow_origin_ptr __msan_metadata_ptr_for_load_##size(      \
		void *addr);                                               \
	struct shadow_origin_ptr __msan_metadata_ptr_for_load_##size(      \
		void *addr)                                                \
	{                                                                  \
		return get_shadow_origin_ptr(addr, size,   false); \
	}                                                                  \
	EXPORT_SYMBOL(__msan_metadata_ptr_for_load_##size);                \
	struct shadow_origin_ptr __msan_metadata_ptr_for_store_##size(     \
		void *addr);                                               \
	struct shadow_origin_ptr __msan_metadata_ptr_for_store_##size(     \
		void *addr)                                                \
	{                                                                  \
		return get_shadow_origin_ptr(addr, size,   true);  \
	}                                                                  \
	EXPORT_SYMBOL(__msan_metadata_ptr_for_store_##size)

DECLARE_METADATA_PTR_GETTER(1);
DECLARE_METADATA_PTR_GETTER(2);
DECLARE_METADATA_PTR_GETTER(4);
DECLARE_METADATA_PTR_GETTER(8);

 
void __msan_instrument_asm_store(void *addr, uintptr_t size);
void __msan_instrument_asm_store(void *addr, uintptr_t size)
{
	unsigned long ua_flags;

	if (!kmsan_enabled)
		return;

	ua_flags = user_access_save();
	 
	if (size > 512) {
		WARN_ONCE(1, "assembly store size too big: %ld\n", size);
		size = 8;
	}
	if (is_bad_asm_addr(addr, size,   true)) {
		user_access_restore(ua_flags);
		return;
	}
	 
	kmsan_internal_unpoison_memory(addr, size,   false);
	user_access_restore(ua_flags);
}
EXPORT_SYMBOL(__msan_instrument_asm_store);

 
static inline void get_param0_metadata(u64 *shadow,
				       depot_stack_handle_t *origin)
{
	struct kmsan_ctx *ctx = kmsan_get_context();

	*shadow = *(u64 *)(ctx->cstate.param_tls);
	*origin = ctx->cstate.param_origin_tls[0];
}

static inline void set_retval_metadata(u64 shadow, depot_stack_handle_t origin)
{
	struct kmsan_ctx *ctx = kmsan_get_context();

	*(u64 *)(ctx->cstate.retval_tls) = shadow;
	ctx->cstate.retval_origin_tls = origin;
}

 
void *__msan_memmove(void *dst, const void *src, uintptr_t n);
void *__msan_memmove(void *dst, const void *src, uintptr_t n)
{
	depot_stack_handle_t origin;
	void *result;
	u64 shadow;

	get_param0_metadata(&shadow, &origin);
	result = __memmove(dst, src, n);
	if (!n)
		 
		return result;
	if (!kmsan_enabled || kmsan_in_runtime())
		return result;

	kmsan_enter_runtime();
	kmsan_internal_memmove_metadata(dst, (void *)src, n);
	kmsan_leave_runtime();

	set_retval_metadata(shadow, origin);
	return result;
}
EXPORT_SYMBOL(__msan_memmove);

 
void *__msan_memcpy(void *dst, const void *src, uintptr_t n);
void *__msan_memcpy(void *dst, const void *src, uintptr_t n)
{
	depot_stack_handle_t origin;
	void *result;
	u64 shadow;

	get_param0_metadata(&shadow, &origin);
	result = __memcpy(dst, src, n);
	if (!n)
		 
		return result;

	if (!kmsan_enabled || kmsan_in_runtime())
		return result;

	kmsan_enter_runtime();
	 
	kmsan_internal_memmove_metadata(dst, (void *)src, n);
	kmsan_leave_runtime();

	set_retval_metadata(shadow, origin);
	return result;
}
EXPORT_SYMBOL(__msan_memcpy);

 
void *__msan_memset(void *dst, int c, uintptr_t n);
void *__msan_memset(void *dst, int c, uintptr_t n)
{
	depot_stack_handle_t origin;
	void *result;
	u64 shadow;

	get_param0_metadata(&shadow, &origin);
	result = __memset(dst, c, n);
	if (!kmsan_enabled || kmsan_in_runtime())
		return result;

	kmsan_enter_runtime();
	 
	kmsan_internal_unpoison_memory(dst, n,   false);
	kmsan_leave_runtime();

	set_retval_metadata(shadow, origin);
	return result;
}
EXPORT_SYMBOL(__msan_memset);

 
depot_stack_handle_t __msan_chain_origin(depot_stack_handle_t origin);
depot_stack_handle_t __msan_chain_origin(depot_stack_handle_t origin)
{
	depot_stack_handle_t ret = 0;
	unsigned long ua_flags;

	if (!kmsan_enabled || kmsan_in_runtime())
		return ret;

	ua_flags = user_access_save();

	 
	kmsan_enter_runtime();
	ret = kmsan_internal_chain_origin(origin);
	kmsan_leave_runtime();
	user_access_restore(ua_flags);
	return ret;
}
EXPORT_SYMBOL(__msan_chain_origin);

 
void __msan_poison_alloca(void *address, uintptr_t size, char *descr);
void __msan_poison_alloca(void *address, uintptr_t size, char *descr)
{
	depot_stack_handle_t handle;
	unsigned long entries[4];
	unsigned long ua_flags;

	if (!kmsan_enabled || kmsan_in_runtime())
		return;

	ua_flags = user_access_save();
	entries[0] = KMSAN_ALLOCA_MAGIC_ORIGIN;
	entries[1] = (u64)descr;
	entries[2] = (u64)__builtin_return_address(0);
	 
	if (IS_ENABLED(CONFIG_UNWINDER_FRAME_POINTER))
		entries[3] = (u64)__builtin_return_address(1);
	else
		entries[3] = 0;

	 
	kmsan_enter_runtime();
	handle = stack_depot_save(entries, ARRAY_SIZE(entries), __GFP_HIGH);
	kmsan_leave_runtime();

	kmsan_internal_set_shadow_origin(address, size, -1, handle,
					   true);
	user_access_restore(ua_flags);
}
EXPORT_SYMBOL(__msan_poison_alloca);

 
void __msan_unpoison_alloca(void *address, uintptr_t size);
void __msan_unpoison_alloca(void *address, uintptr_t size)
{
	if (!kmsan_enabled || kmsan_in_runtime())
		return;

	kmsan_enter_runtime();
	kmsan_internal_unpoison_memory(address, size,   true);
	kmsan_leave_runtime();
}
EXPORT_SYMBOL(__msan_unpoison_alloca);

 
void __msan_warning(u32 origin);
void __msan_warning(u32 origin)
{
	if (!kmsan_enabled || kmsan_in_runtime())
		return;
	kmsan_enter_runtime();
	kmsan_report(origin,   0,   0,
		       0,   0,   0,
		     REASON_ANY);
	kmsan_leave_runtime();
}
EXPORT_SYMBOL(__msan_warning);

 
struct kmsan_context_state *__msan_get_context_state(void);
struct kmsan_context_state *__msan_get_context_state(void)
{
	return &kmsan_get_context()->cstate;
}
EXPORT_SYMBOL(__msan_get_context_state);
