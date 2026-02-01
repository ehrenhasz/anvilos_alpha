 

#ifndef __DC_PANEL_CNTL__DCN31_H__
#define __DC_PANEL_CNTL__DCN31_H__

#include "panel_cntl.h"
#include "dce/dce_panel_cntl.h"

struct dcn31_panel_cntl {
	struct panel_cntl base;
};

void dcn31_panel_cntl_construct(
	struct dcn31_panel_cntl *dcn31_panel_cntl,
	const struct panel_cntl_init_data *init_data);

#endif  
