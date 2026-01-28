#ifndef _SCI_SAS_H_
#define _SCI_SAS_H_
#include <linux/kernel.h>
#define FIS_REGH2D          0x27
#define FIS_REGD2H          0x34
#define FIS_SETDEVBITS      0xA1
#define FIS_DMA_ACTIVATE    0x39
#define FIS_DMA_SETUP       0x41
#define FIS_BIST_ACTIVATE   0x58
#define FIS_PIO_SETUP       0x5F
#define FIS_DATA            0x46
#define SSP_RESP_IU_MAX_SIZE	280
struct ssp_cmd_iu {
	u8 LUN[8];
	u8 add_cdb_len:6;
	u8 _r_a:2;
	u8 _r_b;
	u8 en_fburst:1;
	u8 task_prio:4;
	u8 task_attr:3;
	u8 _r_c;
	u8 cdb[16];
}  __packed;
struct ssp_task_iu {
	u8 LUN[8];
	u8 _r_a;
	u8 task_func;
	u8 _r_b[4];
	u16 task_tag;
	u8 _r_c[12];
}  __packed;
struct smp_req_phy_id {
	u8 _r_a[4];		 
	u8 ign_zone_grp:1;	 
	u8 _r_b:7;
	u8 phy_id;		 
	u8 _r_c;		 
	u8 _r_d;		 
}  __packed;
struct smp_req_conf_rtinfo {
	u16 exp_change_cnt;		 
	u8 exp_rt_idx_hi;		 
	u8 exp_rt_idx;			 
	u8 _r_a;			 
	u8 phy_id;			 
	u16 _r_b;			 
	u8 _r_c:7;			 
	u8 dis_rt_entry:1;
	u8 _r_d[3];			 
	u8 rt_sas_addr[8];		 
	u8 _r_e[16];			 
}  __packed;
struct smp_req_phycntl {
	u16 exp_change_cnt;		 
	u8 _r_a[3];			 
	u8 phy_id;			 
	u8 phy_op;			 
	u8 upd_pathway:1;		 
	u8 _r_b:7;
	u8 _r_c[12];			 
	u8 att_dev_name[8];              
	u8 _r_d:4;			 
	u8 min_linkrate:4;
	u8 _r_e:4;			 
	u8 max_linkrate:4;
	u8 _r_f[2];			 
	u8 pathway:4;			 
	u8 _r_g:4;
	u8 _r_h[3];			 
}  __packed;
struct smp_req {
	u8 type;		 
	u8 func;		 
	u8 alloc_resp_len;	 
	u8 req_len;		 
	u8 req_data[];
}  __packed;
struct sci_sas_address {
	u32 high;
	u32 low;
};
#endif
