 

#ifndef _BRCM_CHANNEL_H_
#define _BRCM_CHANNEL_H_

 
#define BRCMS_TXPWR_DB_FACTOR 4

 
#define BRCMS_PEAK_CONDUCTED	0x00	 
#define BRCMS_EIRP		0x01	 
#define BRCMS_DFS_TPC		0x02	 
#define BRCMS_NO_OFDM		0x04	 
#define BRCMS_NO_40MHZ		0x08	 
#define BRCMS_NO_MIMO		0x10	 
#define BRCMS_RADAR_TYPE_EU       0x20	 
#define BRCMS_DFS_FCC             BRCMS_DFS_TPC	 

#define BRCMS_DFS_EU (BRCMS_DFS_TPC | BRCMS_RADAR_TYPE_EU)  

struct brcms_cm_info *brcms_c_channel_mgr_attach(struct brcms_c_info *wlc);

void brcms_c_channel_mgr_detach(struct brcms_cm_info *wlc_cm);

bool brcms_c_valid_chanspec_db(struct brcms_cm_info *wlc_cm, u16 chspec);

void brcms_c_channel_reg_limits(struct brcms_cm_info *wlc_cm, u16 chanspec,
				struct txpwr_limits *txpwr);
void brcms_c_channel_set_chanspec(struct brcms_cm_info *wlc_cm, u16 chanspec,
				  u8 local_constraint_qdbm);
void brcms_c_regd_init(struct brcms_c_info *wlc);

#endif				 
