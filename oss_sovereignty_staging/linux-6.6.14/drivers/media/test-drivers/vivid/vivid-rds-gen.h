 
 

#ifndef _VIVID_RDS_GEN_H_
#define _VIVID_RDS_GEN_H_

 
#define VIVID_RDS_GEN_GROUPS 57
#define VIVID_RDS_GEN_BLKS_PER_GRP 4
#define VIVID_RDS_GEN_BLOCKS (VIVID_RDS_GEN_BLKS_PER_GRP * VIVID_RDS_GEN_GROUPS)
#define VIVID_RDS_NSEC_PER_BLK (u32)(5ull * NSEC_PER_SEC / VIVID_RDS_GEN_BLOCKS)

struct vivid_rds_gen {
	struct v4l2_rds_data	data[VIVID_RDS_GEN_BLOCKS];
	bool			use_rbds;
	u16			picode;
	u8			pty;
	bool			mono_stereo;
	bool			art_head;
	bool			compressed;
	bool			dyn_pty;
	bool			ta;
	bool			tp;
	bool			ms;
	char			psname[8 + 1];
	char			radiotext[64 + 1];
};

void vivid_rds_gen_fill(struct vivid_rds_gen *rds, unsigned freq,
		    bool use_alternate);
void vivid_rds_generate(struct vivid_rds_gen *rds);

#endif
