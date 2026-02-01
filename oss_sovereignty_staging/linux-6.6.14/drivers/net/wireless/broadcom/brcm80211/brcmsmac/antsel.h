 

#ifndef _BRCM_ANTSEL_H_
#define _BRCM_ANTSEL_H_

struct antsel_info *brcms_c_antsel_attach(struct brcms_c_info *wlc);
void brcms_c_antsel_detach(struct antsel_info *asi);
void brcms_c_antsel_init(struct antsel_info *asi);
void brcms_c_antsel_antcfg_get(struct antsel_info *asi, bool usedef, bool sel,
			       u8 id, u8 fbid, u8 *antcfg, u8 *fbantcfg);
u8 brcms_c_antsel_antsel2id(struct antsel_info *asi, u16 antsel);

#endif  
