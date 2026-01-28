#ifndef _DDBRIDGE_MAX_H_
#define _DDBRIDGE_MAX_H_
#include "ddbridge.h"
int ddb_lnb_init_fmode(struct ddb *dev, struct ddb_link *link, u32 fm);
int ddb_fe_attach_mxl5xx(struct ddb_input *input);
int ddb_fe_attach_mci(struct ddb_input *input, u32 type);
#endif  
