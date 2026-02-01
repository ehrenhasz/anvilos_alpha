 
 

#ifndef __IWLWIFI_DEVICE_TRACE
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include "iwl-trans.h"
#if !defined(__IWLWIFI_DEVICE_TRACE)
static inline bool iwl_trace_data(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (void *)skb->data;
	__le16 fc = hdr->frame_control;
	int offs = 24;  

	if (!ieee80211_is_data(fc))
		return false;

	 

	if (ieee80211_has_a4(fc))
		offs += 6;
	if (ieee80211_is_data_qos(fc))
		offs += 2;
	 

	 
	offs += 6;

	return skb->len <= offs + 2 ||
		*(__be16 *)(skb->data + offs) != cpu_to_be16(ETH_P_PAE);
}

static inline size_t iwl_rx_trace_len(const struct iwl_trans *trans,
				      void *rxbuf, size_t len,
				      size_t *out_hdr_offset)
{
	struct iwl_cmd_header *cmd = (void *)((u8 *)rxbuf + sizeof(__le32));
	struct ieee80211_hdr *hdr = NULL;
	size_t hdr_offset;

	if (cmd->cmd != trans->rx_mpdu_cmd)
		return len;

	hdr_offset = sizeof(struct iwl_cmd_header) +
		     trans->rx_mpdu_cmd_hdr_size;

	if (out_hdr_offset)
		*out_hdr_offset = hdr_offset;

	hdr = (void *)((u8 *)cmd + hdr_offset);
	if (!ieee80211_is_data(hdr->frame_control))
		return len;
	 
	return sizeof(__le32) + sizeof(*cmd) + trans->rx_mpdu_cmd_hdr_size +
		ieee80211_hdrlen(hdr->frame_control);
}
#endif

#define __IWLWIFI_DEVICE_TRACE

#include <linux/tracepoint.h>
#include <linux/device.h>


#if !defined(CONFIG_IWLWIFI_DEVICE_TRACING) || defined(__CHECKER__)
#undef TRACE_EVENT
#define TRACE_EVENT(name, proto, ...) \
static inline void trace_ ## name(proto) {}
#undef DECLARE_EVENT_CLASS
#define DECLARE_EVENT_CLASS(...)
#undef DEFINE_EVENT
#define DEFINE_EVENT(evt_class, name, proto, ...) \
static inline void trace_ ## name(proto) {}
#endif

#define DEV_ENTRY	__string(dev, dev_name(dev))
#define DEV_ASSIGN	__assign_str(dev, dev_name(dev))

#include "iwl-devtrace-io.h"
#include "iwl-devtrace-ucode.h"
#include "iwl-devtrace-msg.h"
#include "iwl-devtrace-data.h"
#include "iwl-devtrace-iwlwifi.h"

#endif  
