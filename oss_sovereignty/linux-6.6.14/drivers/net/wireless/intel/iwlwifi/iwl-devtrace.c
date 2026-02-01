
 

#include <linux/module.h>

 
#ifndef __CHECKER__
#include "iwl-trans.h"

#define CREATE_TRACE_POINTS
#ifdef CONFIG_CC_IS_GCC
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif
#include "iwl-devtrace.h"

EXPORT_TRACEPOINT_SYMBOL(iwlwifi_dev_ucode_event);
EXPORT_TRACEPOINT_SYMBOL(iwlwifi_dev_ucode_cont_event);
EXPORT_TRACEPOINT_SYMBOL(iwlwifi_dev_ucode_wrap_event);
#endif
