 
 

#ifndef _CXLFLASH_SUPERPIPE_H
#define _CXLFLASH_SUPERPIPE_H

extern struct cxlflash_global global;

 

 
#define MC_CHUNK_SIZE     (1 << MC_RHT_NMASK)	 

#define CMD_TIMEOUT 30   
#define CMD_RETRIES 5    

#define MAX_SECTOR_UNIT  512  

enum lun_mode {
	MODE_NONE = 0,
	MODE_VIRTUAL,
	MODE_PHYSICAL
};

 
struct glun_info {
	u64 max_lba;		 
	u32 blk_len;		 
	enum lun_mode mode;	 
	int users;		 

	u8 wwid[16];

	struct mutex mutex;

	struct blka blka;
	struct list_head list;
};

 
struct llun_info {
	u64 lun_id[MAX_FC_PORTS];  
	u32 lun_index;		 
	u32 host_no;		 
	u32 port_sel;		 
	bool in_table;		 

	u8 wwid[16];		 

	struct glun_info *parent;  
	struct scsi_device *sdev;
	struct list_head list;
};

struct lun_access {
	struct llun_info *lli;
	struct scsi_device *sdev;
	struct list_head list;
};

enum ctx_ctrl {
	CTX_CTRL_CLONE		= (1 << 1),
	CTX_CTRL_ERR		= (1 << 2),
	CTX_CTRL_ERR_FALLBACK	= (1 << 3),
	CTX_CTRL_NOPID		= (1 << 4),
	CTX_CTRL_FILE		= (1 << 5)
};

#define ENCODE_CTXID(_ctx, _id)	(((((u64)_ctx) & 0xFFFFFFFF0ULL) << 28) | _id)
#define DECODE_CTXID(_val)	(_val & 0xFFFFFFFF)

struct ctx_info {
	struct sisl_ctrl_map __iomem *ctrl_map;  
	struct sisl_rht_entry *rht_start;  
	u32 rht_out;		 
	u32 rht_perms;		 
	struct llun_info **rht_lun;        
	u8 *rht_needs_ws;	 

	u64 ctxid;
	u64 irqs;  
	pid_t pid;
	bool initialized;
	bool unavail;
	bool err_recovery_active;
	struct mutex mutex;  
	struct kref kref;
	void *ctx;
	struct cxlflash_cfg *cfg;
	struct list_head luns;	 
	const struct vm_operations_struct *cxl_mmap_vmops;
	struct file *file;
	struct list_head list;  
};

struct cxlflash_global {
	struct mutex mutex;
	struct list_head gluns; 
	struct page *err_page;  
};

int cxlflash_vlun_resize(struct scsi_device *sdev,
			 struct dk_cxlflash_resize *resize);
int _cxlflash_vlun_resize(struct scsi_device *sdev, struct ctx_info *ctxi,
			  struct dk_cxlflash_resize *resize);

int cxlflash_disk_release(struct scsi_device *sdev,
			  struct dk_cxlflash_release *release);
int _cxlflash_disk_release(struct scsi_device *sdev, struct ctx_info *ctxi,
			   struct dk_cxlflash_release *release);

int cxlflash_disk_clone(struct scsi_device *sdev,
			struct dk_cxlflash_clone *clone);

int cxlflash_disk_virtual_open(struct scsi_device *sdev, void *arg);

int cxlflash_lun_attach(struct glun_info *gli, enum lun_mode mode, bool locked);
void cxlflash_lun_detach(struct glun_info *gli);

struct ctx_info *get_context(struct cxlflash_cfg *cfg, u64 rctxit, void *arg,
			     enum ctx_ctrl ctrl);
void put_context(struct ctx_info *ctxi);

struct sisl_rht_entry *get_rhte(struct ctx_info *ctxi, res_hndl_t rhndl,
				struct llun_info *lli);

struct sisl_rht_entry *rhte_checkout(struct ctx_info *ctxi,
				     struct llun_info *lli);
void rhte_checkin(struct ctx_info *ctxi, struct sisl_rht_entry *rhte);

void cxlflash_ba_terminate(struct ba_lun *ba_lun);

int cxlflash_manage_lun(struct scsi_device *sdev,
			struct dk_cxlflash_manage_lun *manage);

int check_state(struct cxlflash_cfg *cfg);

#endif  
