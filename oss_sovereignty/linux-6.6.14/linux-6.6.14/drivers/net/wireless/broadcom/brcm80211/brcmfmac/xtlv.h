#ifndef __BRCMF_XTLV_H
#define __BRCMF_XTLV_H
#include <linux/types.h>
#include <linux/bits.h>
struct brcmf_xtlv {
	u16 id;
	u16 len;
	u8 data[];
};
enum brcmf_xtlv_option {
	BRCMF_XTLV_OPTION_ALIGN32 = BIT(0),
	BRCMF_XTLV_OPTION_IDU8 = BIT(1),
	BRCMF_XTLV_OPTION_LENU8 = BIT(2),
};
int brcmf_xtlv_data_size(int dlen, u16 opts);
void brcmf_xtlv_pack_header(struct brcmf_xtlv *xtlv, u16 id, u16 len,
			    const u8 *data, u16 opts);
#endif  
