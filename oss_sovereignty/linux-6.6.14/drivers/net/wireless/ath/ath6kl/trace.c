 

#include <linux/module.h>

#define CREATE_TRACE_POINTS
#include "trace.h"

EXPORT_TRACEPOINT_SYMBOL(ath6kl_sdio);
EXPORT_TRACEPOINT_SYMBOL(ath6kl_sdio_scat);
