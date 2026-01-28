#ifndef _ASM_POWERPC_VAS_H
#define _ASM_POWERPC_VAS_H
#include <linux/sched/mm.h>
#include <linux/mmu_context.h>
#include <asm/icswx.h>
#include <uapi/asm/vas-api.h>
#define VAS_RX_FIFO_SIZE_MIN	(1 << 10)	 
#define VAS_RX_FIFO_SIZE_MAX	(8 << 20)	 
#define VAS_THRESH_DISABLED		0
#define VAS_THRESH_FIFO_GT_HALF_FULL	1
#define VAS_THRESH_FIFO_GT_QTR_FULL	2
#define VAS_THRESH_FIFO_GT_EIGHTH_FULL	3
#define VAS_WIN_ACTIVE		0x0	 
#define VAS_WIN_NO_CRED_CLOSE	0x00000001
#define VAS_WIN_MIGRATE_CLOSE	0x00000002
#define GET_FIELD(m, v)                (((v) & (m)) >> MASK_LSH(m))
#define MASK_LSH(m)            (__builtin_ffsl(m) - 1)
#define SET_FIELD(m, v, val)   \
		(((v) & ~(m)) | ((((typeof(v))(val)) << MASK_LSH(m)) & (m)))
enum vas_cop_type {
	VAS_COP_TYPE_FAULT,
	VAS_COP_TYPE_842,
	VAS_COP_TYPE_842_HIPRI,
	VAS_COP_TYPE_GZIP,
	VAS_COP_TYPE_GZIP_HIPRI,
	VAS_COP_TYPE_FTW,
	VAS_COP_TYPE_MAX,
};
struct vas_user_win_ref {
	struct pid *pid;	 
	struct pid *tgid;	 
	struct mm_struct *mm;	 
	struct mutex mmap_mutex;	 
	struct vm_area_struct *vma;	 
};
struct vas_window {
	u32 winid;
	u32 wcreds_max;	 
	u32 status;	 
	enum vas_cop_type cop;
	struct vas_user_win_ref task_ref;
	char *dbgname;
	struct dentry *dbgdir;
};
struct vas_user_win_ops {
	struct vas_window * (*open_win)(int vas_id, u64 flags,
				enum vas_cop_type);
	u64 (*paste_addr)(struct vas_window *);
	int (*close_win)(struct vas_window *);
};
static inline void put_vas_user_win_ref(struct vas_user_win_ref *ref)
{
	put_pid(ref->pid);
	put_pid(ref->tgid);
	if (ref->mm)
		mmdrop(ref->mm);
}
static inline void vas_user_win_add_mm_context(struct vas_user_win_ref *ref)
{
	mm_context_add_vas_window(ref->mm);
	asm volatile(PPC_CP_ABORT);
}
struct vas_rx_win_attr {
	u64 rx_fifo;
	int rx_fifo_size;
	int wcreds_max;
	bool pin_win;
	bool rej_no_credit;
	bool tx_wcred_mode;
	bool rx_wcred_mode;
	bool tx_win_ord_mode;
	bool rx_win_ord_mode;
	bool data_stamp;
	bool nx_win;
	bool fault_win;
	bool user_win;
	bool notify_disable;
	bool intr_disable;
	bool notify_early;
	int lnotify_lpid;
	int lnotify_pid;
	int lnotify_tid;
	u32 pswid;
	int tc_mode;
};
struct vas_tx_win_attr {
	enum vas_cop_type cop;
	int wcreds_max;
	int lpid;
	int pidr;		 
	int pswid;
	int rsvd_txbuf_count;
	int tc_mode;
	bool user_win;
	bool pin_win;
	bool rej_no_credit;
	bool rsvd_txbuf_enable;
	bool tx_wcred_mode;
	bool rx_wcred_mode;
	bool tx_win_ord_mode;
	bool rx_win_ord_mode;
};
#ifdef CONFIG_PPC_POWERNV
int chip_to_vas_id(int chipid);
void vas_init_rx_win_attr(struct vas_rx_win_attr *rxattr, enum vas_cop_type cop);
struct vas_window *vas_rx_win_open(int vasid, enum vas_cop_type cop,
				   struct vas_rx_win_attr *attr);
extern void vas_init_tx_win_attr(struct vas_tx_win_attr *txattr,
			enum vas_cop_type cop);
struct vas_window *vas_tx_win_open(int vasid, enum vas_cop_type cop,
			struct vas_tx_win_attr *attr);
int vas_win_close(struct vas_window *win);
int vas_copy_crb(void *crb, int offset);
int vas_paste_crb(struct vas_window *win, int offset, bool re);
int vas_register_api_powernv(struct module *mod, enum vas_cop_type cop_type,
			     const char *name);
void vas_unregister_api_powernv(void);
#endif
#ifdef CONFIG_PPC_PSERIES
#define VAS_GZIP_QOS_FEAT	0x1
#define VAS_GZIP_DEF_FEAT	0x2
#define VAS_GZIP_QOS_FEAT_BIT	PPC_BIT(VAS_GZIP_QOS_FEAT)  
#define VAS_GZIP_DEF_FEAT_BIT	PPC_BIT(VAS_GZIP_DEF_FEAT)  
#define VAS_NX_GZIP_FEAT	0x1
#define VAS_NX_GZIP_FEAT_BIT	PPC_BIT(VAS_NX_GZIP_FEAT)  
struct hv_vas_all_caps {
	__be64  descriptor;
	__be64  feat_type;
} __packed __aligned(0x1000);
struct vas_all_caps {
	u64     descriptor;
	u64     feat_type;
};
int h_query_vas_capabilities(const u64 hcall, u8 query_type, u64 result);
int vas_register_api_pseries(struct module *mod,
			     enum vas_cop_type cop_type, const char *name);
void vas_unregister_api_pseries(void);
#endif
int vas_register_coproc_api(struct module *mod, enum vas_cop_type cop_type,
			    const char *name,
			    const struct vas_user_win_ops *vops);
void vas_unregister_coproc_api(void);
int get_vas_user_win_ref(struct vas_user_win_ref *task_ref);
void vas_update_csb(struct coprocessor_request_block *crb,
		    struct vas_user_win_ref *task_ref);
void vas_dump_crb(struct coprocessor_request_block *crb);
#endif  
