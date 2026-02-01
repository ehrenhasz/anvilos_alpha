 
 
#ifndef WFX_DEBUG_H
#define WFX_DEBUG_H

struct wfx_dev;

int wfx_debug_init(struct wfx_dev *wdev);

const char *wfx_get_hif_name(unsigned long id);
const char *wfx_get_mib_name(unsigned long id);
const char *wfx_get_reg_name(unsigned long id);

#endif
