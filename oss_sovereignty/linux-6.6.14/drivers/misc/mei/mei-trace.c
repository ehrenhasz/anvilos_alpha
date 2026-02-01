
 
#include <linux/module.h>

 
#ifndef __CHECKER__
#define CREATE_TRACE_POINTS
#include "mei-trace.h"

EXPORT_TRACEPOINT_SYMBOL(mei_reg_read);
EXPORT_TRACEPOINT_SYMBOL(mei_reg_write);
EXPORT_TRACEPOINT_SYMBOL(mei_pci_cfg_read);
#endif  
