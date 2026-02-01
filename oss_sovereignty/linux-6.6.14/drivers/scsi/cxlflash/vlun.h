 
 

#ifndef _CXLFLASH_VLUN_H
#define _CXLFLASH_VLUN_H

 
#define MC_RHT_NMASK      16	 
#define MC_CHUNK_SHIFT    MC_RHT_NMASK	 

#define HIBIT             (BITS_PER_LONG - 1)

#define MAX_AUN_CLONE_CNT 0xFF

 
#define LXT_GROUP_SIZE          8
#define LXT_NUM_GROUPS(lxt_cnt) (((lxt_cnt) + 7)/8)	 
#define LXT_LUNIDX_SHIFT  8	 
#define LXT_PERM_SHIFT    4	 

struct ba_lun_info {
	u64 *lun_alloc_map;
	u32 lun_bmap_size;
	u32 total_aus;
	u64 free_aun_cnt;

	 
	u32 free_low_idx;
	u32 free_curr_idx;
	u32 free_high_idx;

	u8 *aun_clone_map;
};

struct ba_lun {
	u64 lun_id;
	u64 wwpn;
	size_t lsize;		 
	size_t lba_size;	 
	size_t au_size;		 
	struct ba_lun_info *ba_lun_handle;
};

 
struct blka {
	struct ba_lun ba_lun;
	u64 nchunk;		 
	struct mutex mutex;
};

#endif  
