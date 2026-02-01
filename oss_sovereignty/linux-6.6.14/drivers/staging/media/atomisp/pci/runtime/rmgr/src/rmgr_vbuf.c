
 

#include "hmm.h"
#include "ia_css_rmgr.h"

#include <type_support.h>
#include <assert_support.h>
#include <platform_support.h>  
#include <ia_css_debug.h>

 
#define NUM_HANDLES 1000
static struct ia_css_rmgr_vbuf_handle handle_table[NUM_HANDLES];

 
static struct ia_css_rmgr_vbuf_pool refpool;

 
static struct ia_css_rmgr_vbuf_pool writepool = {
	.copy_on_write	= true,
};

 
static struct ia_css_rmgr_vbuf_pool hmmbufferpool = {
	.copy_on_write	= true,
	.recycle	= true,
	.size		= 32,
};

struct ia_css_rmgr_vbuf_pool *vbuf_ref = &refpool;
struct ia_css_rmgr_vbuf_pool *vbuf_write = &writepool;
struct ia_css_rmgr_vbuf_pool *hmm_buffer_pool = &hmmbufferpool;

 
static void rmgr_refcount_init_vbuf(void)
{
	 
	memset(&handle_table, 0, sizeof(handle_table));
}

 
void ia_css_rmgr_refcount_retain_vbuf(struct ia_css_rmgr_vbuf_handle **handle)
{
	int i;
	struct ia_css_rmgr_vbuf_handle *h;

	if ((!handle) || (!*handle)) {
		IA_CSS_LOG("Invalid inputs");
		return;
	}
	 
	if ((*handle)->count == 0) {
		h = *handle;
		*handle = NULL;
		for (i = 0; i < NUM_HANDLES; i++) {
			if (handle_table[i].count == 0) {
				*handle = &handle_table[i];
				break;
			}
		}
		 
		if (!*handle) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR,
					    "ia_css_i_host_refcount_retain_vbuf() failed to find empty slot!\n");
			return;
		}
		(*handle)->vptr = h->vptr;
		(*handle)->size = h->size;
	}
	(*handle)->count++;
}

 
void ia_css_rmgr_refcount_release_vbuf(struct ia_css_rmgr_vbuf_handle **handle)
{
	if ((!handle) || ((*handle) == NULL) || (((*handle)->count) == 0)) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR, "%s invalid arguments!\n", __func__);
		return;
	}
	 
	(*handle)->count--;
	 
	if ((*handle)->count == 0) {
		(*handle)->vptr = 0x0;
		(*handle)->size = 0;
		*handle = NULL;
	}
}

 
int ia_css_rmgr_init_vbuf(struct ia_css_rmgr_vbuf_pool *pool)
{
	int err = 0;
	size_t bytes_needed;

	rmgr_refcount_init_vbuf();
	assert(pool);
	if (!pool)
		return -EINVAL;
	 
	if (pool->recycle && pool->size) {
		 
		bytes_needed =
		    sizeof(void *) *
		    pool->size;
		pool->handles = kvmalloc(bytes_needed, GFP_KERNEL);
		if (pool->handles)
			memset(pool->handles, 0, bytes_needed);
		else
			err = -ENOMEM;
	} else {
		 
		pool->size = 0;
		pool->handles = NULL;
	}
	return err;
}

 
void ia_css_rmgr_uninit_vbuf(struct ia_css_rmgr_vbuf_pool *pool)
{
	u32 i;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "%s\n", __func__);
	if (!pool) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR, "%s NULL argument\n", __func__);
		return;
	}
	if (pool->handles) {
		 
		for (i = 0; i < pool->size; i++) {
			if (pool->handles[i]) {
				ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
						    "   freeing/releasing %x (count=%d)\n",
						    pool->handles[i]->vptr,
						    pool->handles[i]->count);
				 
				hmm_free(pool->handles[i]->vptr);
				 
				ia_css_rmgr_refcount_release_vbuf(&pool->handles[i]);
			}
		}
		 
		kvfree(pool->handles);
		pool->handles = NULL;
	}
}

 
static
void rmgr_push_handle(struct ia_css_rmgr_vbuf_pool *pool,
		      struct ia_css_rmgr_vbuf_handle **handle)
{
	u32 i;
	bool succes = false;

	assert(pool);
	assert(pool->recycle);
	assert(pool->handles);
	assert(handle);
	for (i = 0; i < pool->size; i++) {
		if (!pool->handles[i]) {
			ia_css_rmgr_refcount_retain_vbuf(handle);
			pool->handles[i] = *handle;
			succes = true;
			break;
		}
	}
	assert(succes);
}

 
static
void rmgr_pop_handle(struct ia_css_rmgr_vbuf_pool *pool,
		     struct ia_css_rmgr_vbuf_handle **handle)
{
	u32 i;

	assert(pool);
	assert(pool->recycle);
	assert(pool->handles);
	assert(handle);
	assert(*handle);
	for (i = 0; i < pool->size; i++) {
		if ((pool->handles[i]) &&
		    (pool->handles[i]->size == (*handle)->size)) {
			*handle = pool->handles[i];
			pool->handles[i] = NULL;
			 
			return;
		}
	}
}

 
void ia_css_rmgr_acq_vbuf(struct ia_css_rmgr_vbuf_pool *pool,
			  struct ia_css_rmgr_vbuf_handle **handle)
{
	if ((!pool) || (!handle) || (!*handle)) {
		IA_CSS_LOG("Invalid inputs");
		return;
	}

	if (pool->copy_on_write) {
		struct ia_css_rmgr_vbuf_handle *new_handle;
		struct ia_css_rmgr_vbuf_handle h = { 0 };

		 
		if ((*handle)->count == 1)
			return;
		 
		if ((*handle)->count > 1) {
			 
			h.vptr = 0x0;
			h.size = (*handle)->size;
			 
			ia_css_rmgr_refcount_release_vbuf(handle);
			new_handle = &h;
		} else {
			new_handle = *handle;
		}
		 
		if (new_handle->vptr == 0x0) {
			if (pool->recycle) {
				 
				rmgr_pop_handle(pool, &new_handle);
			}
			if (new_handle->vptr == 0x0) {
				 
				new_handle->vptr = hmm_alloc(new_handle->size);
			} else {
				 
				*handle = new_handle;
				return;
			}
		}
		 
		ia_css_rmgr_refcount_retain_vbuf(&new_handle);
		*handle = new_handle;
		return;
	}
	 
	ia_css_rmgr_refcount_retain_vbuf(handle);
}

 
void ia_css_rmgr_rel_vbuf(struct ia_css_rmgr_vbuf_pool *pool,
			  struct ia_css_rmgr_vbuf_handle **handle)
{
	if ((!pool) || (!handle) || (!*handle)) {
		IA_CSS_LOG("Invalid inputs");
		return;
	}
	 
	if ((*handle)->count == 1) {
		if (!pool->recycle) {
			 
			hmm_free((*handle)->vptr);
		} else {
			 
			rmgr_push_handle(pool, handle);
		}
	}
	ia_css_rmgr_refcount_release_vbuf(handle);
	*handle = NULL;
}
