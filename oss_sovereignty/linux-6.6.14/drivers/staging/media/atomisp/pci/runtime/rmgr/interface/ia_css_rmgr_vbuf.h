 
 

#ifndef _IA_CSS_RMGR_VBUF_H
#define _IA_CSS_RMGR_VBUF_H

#include "ia_css_rmgr.h"
#include <type_support.h>
#include <ia_css_types.h>
#include <system_local.h>

 
struct ia_css_rmgr_vbuf_handle {
	ia_css_ptr vptr;
	u8 count;
	u32 size;
};

 
struct ia_css_rmgr_vbuf_pool {
	u8 copy_on_write;
	u8 recycle;
	u32 size;
	u32 index;
	struct ia_css_rmgr_vbuf_handle **handles;
};

 
extern struct ia_css_rmgr_vbuf_pool *vbuf_ref;
extern struct ia_css_rmgr_vbuf_pool *vbuf_write;
extern struct ia_css_rmgr_vbuf_pool *hmm_buffer_pool;

 
STORAGE_CLASS_RMGR_H int ia_css_rmgr_init_vbuf(
    struct ia_css_rmgr_vbuf_pool *pool);

 
STORAGE_CLASS_RMGR_H void ia_css_rmgr_uninit_vbuf(
    struct ia_css_rmgr_vbuf_pool *pool);

 
STORAGE_CLASS_RMGR_H void ia_css_rmgr_acq_vbuf(
    struct ia_css_rmgr_vbuf_pool *pool,
    struct ia_css_rmgr_vbuf_handle **handle);

 
STORAGE_CLASS_RMGR_H void ia_css_rmgr_rel_vbuf(
    struct ia_css_rmgr_vbuf_pool *pool,
    struct ia_css_rmgr_vbuf_handle **handle);

 
void ia_css_rmgr_refcount_retain_vbuf(struct ia_css_rmgr_vbuf_handle **handle);

 
void ia_css_rmgr_refcount_release_vbuf(struct ia_css_rmgr_vbuf_handle **handle);

#endif	 
