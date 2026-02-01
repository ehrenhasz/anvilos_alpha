
 

#include "ia_css_rmgr.h"

int ia_css_rmgr_init(void)
{
	int err = 0;

	err = ia_css_rmgr_init_vbuf(vbuf_ref);
	if (!err)
		err = ia_css_rmgr_init_vbuf(vbuf_write);
	if (!err)
		err = ia_css_rmgr_init_vbuf(hmm_buffer_pool);
	if (err)
		ia_css_rmgr_uninit();
	return err;
}

 
void ia_css_rmgr_uninit(void)
{
	ia_css_rmgr_uninit_vbuf(hmm_buffer_pool);
	ia_css_rmgr_uninit_vbuf(vbuf_write);
	ia_css_rmgr_uninit_vbuf(vbuf_ref);
}
