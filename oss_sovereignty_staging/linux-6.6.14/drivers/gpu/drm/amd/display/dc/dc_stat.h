 

#ifndef _DC_STAT_H_
#define _DC_STAT_H_

 

#include "dc.h"
#include "dmub/dmub_srv.h"

void dc_stat_get_dmub_notification(const struct dc *dc, struct dmub_notification *notify);
void dc_stat_get_dmub_dataout(const struct dc *dc, uint32_t *dataout);

#endif  
