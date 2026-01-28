#ifndef __BRCMS_TRACE_EVENTS_H
#define __BRCMS_TRACE_EVENTS_H
#include <linux/types.h>
#include <linux/device.h>
#include <linux/tracepoint.h>
#include "mac80211_if.h"
#ifndef CONFIG_BRCM_TRACING
#undef TRACE_EVENT
#define TRACE_EVENT(name, proto, ...) \
static inline void trace_ ## name(proto) {}
#undef DECLARE_EVENT_CLASS
#define DECLARE_EVENT_CLASS(...)
#undef DEFINE_EVENT
#define DEFINE_EVENT(evt_class, name, proto, ...) \
static inline void trace_ ## name(proto) {}
#endif
#include "brcms_trace_brcmsmac.h"
#include "brcms_trace_brcmsmac_tx.h"
#include "brcms_trace_brcmsmac_msg.h"
#endif  
