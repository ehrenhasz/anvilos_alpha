 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>

#include "ath.h"
#include "trace.h"

MODULE_AUTHOR("Atheros Communications");
MODULE_DESCRIPTION("Shared library for Atheros wireless LAN cards.");
MODULE_LICENSE("Dual BSD/GPL");

struct sk_buff *ath_rxbuf_alloc(struct ath_common *common,
				u32 len,
				gfp_t gfp_mask)
{
	struct sk_buff *skb;
	u32 off;

	 

	 
	skb = __dev_alloc_skb(len + common->cachelsz - 1, gfp_mask);
	if (skb != NULL) {
		off = ((unsigned long) skb->data) % common->cachelsz;
		if (off != 0)
			skb_reserve(skb, common->cachelsz - off);
	} else {
		pr_err("skbuff alloc of size %u failed\n", len);
		return NULL;
	}

	return skb;
}
EXPORT_SYMBOL(ath_rxbuf_alloc);

bool ath_is_mybeacon(struct ath_common *common, struct ieee80211_hdr *hdr)
{
	return ieee80211_is_beacon(hdr->frame_control) &&
		!is_zero_ether_addr(common->curbssid) &&
		ether_addr_equal_64bits(hdr->addr3, common->curbssid);
}
EXPORT_SYMBOL(ath_is_mybeacon);

void ath_printk(const char *level, const struct ath_common* common,
		const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	if (common && common->hw && common->hw->wiphy) {
		printk("%sath: %s: %pV",
		       level, wiphy_name(common->hw->wiphy), &vaf);
		trace_ath_log(common->hw->wiphy, &vaf);
	} else {
		printk("%sath: %pV", level, &vaf);
	}

	va_end(args);
}
EXPORT_SYMBOL(ath_printk);

const char *ath_bus_type_strings[] = {
	[ATH_PCI] = "pci",
	[ATH_AHB] = "ahb",
	[ATH_USB] = "usb",
};
EXPORT_SYMBOL(ath_bus_type_strings);
