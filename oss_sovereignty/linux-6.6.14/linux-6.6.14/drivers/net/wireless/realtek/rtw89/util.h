#ifndef __RTW89_UTIL_H__
#define __RTW89_UTIL_H__
#include "core.h"
#define rtw89_iterate_vifs_bh(rtwdev, iterator, data)                          \
	ieee80211_iterate_active_interfaces_atomic((rtwdev)->hw,               \
			IEEE80211_IFACE_ITER_NORMAL, iterator, data)
#define rtw89_for_each_rtwvif(rtwdev, rtwvif)				       \
	list_for_each_entry(rtwvif, &(rtwdev)->rtwvifs_list, list)
static inline s32 s32_div_u32_round_down(s32 dividend, u32 divisor, s32 *remainder)
{
	s32 i_divisor = (s32)divisor;
	s32 i_remainder;
	s32 quotient;
	quotient = dividend / i_divisor;
	i_remainder = dividend % i_divisor;
	if (i_remainder < 0) {
		quotient--;
		i_remainder += i_divisor;
	}
	if (remainder)
		*remainder = i_remainder;
	return quotient;
}
static inline s32 s32_div_u32_round_closest(s32 dividend, u32 divisor)
{
	return s32_div_u32_round_down(dividend + divisor / 2, divisor, NULL);
}
static inline void ether_addr_copy_mask(u8 *dst, const u8 *src, u8 mask)
{
	int i;
	eth_zero_addr(dst);
	for (i = 0; i < ETH_ALEN; i++) {
		if (mask & BIT(i))
			dst[i] = src[i];
	}
}
#endif
