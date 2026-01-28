#ifndef _QIB_KERNEL_H
#define _QIB_KERNEL_H
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/kref.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/xarray.h>
#include <rdma/ib_hdrs.h>
#include <rdma/rdma_vt.h>
#include "qib_common.h"
#include "qib_verbs.h"
#define QIB_CHIP_VERS_MAJ 2U
#define QIB_CHIP_VERS_MIN 0U
#define QIB_OUI 0x001175
#define QIB_OUI_LSB 40
struct qlogic_ib_stats {
	__u64 sps_ints;  
	__u64 sps_errints;  
	__u64 sps_txerrs;  
	__u64 sps_rcverrs;  
	__u64 sps_hwerrs;  
	__u64 sps_nopiobufs;  
	__u64 sps_ctxts;  
	__u64 sps_lenerrs;  
	__u64 sps_buffull;
	__u64 sps_hdrfull;
};
extern struct qlogic_ib_stats qib_stats;
extern const struct pci_error_handlers qib_pci_err_handler;
#define QIB_CHIP_SWVERSION QIB_CHIP_VERS_MAJ
#define QIB_TRAFFIC_ACTIVE_THRESHOLD (2000)
#ifdef CONFIG_DEBUG_FS
struct qib_opcode_stats_perctx;
#endif
struct qib_ctxtdata {
	void **rcvegrbuf;
	dma_addr_t *rcvegrbuf_phys;
	void *rcvhdrq;
	void *rcvhdrtail_kvaddr;
	void *tid_pg_list;
	unsigned long *user_event_mask;
	wait_queue_head_t wait;
	dma_addr_t rcvegr_phys;
	dma_addr_t rcvhdrq_phys;
	dma_addr_t rcvhdrqtailaddr_phys;
	int cnt;
	unsigned ctxt;
	int node_id;
	u16 subctxt_cnt;
	u16 subctxt_id;
	u16 rcvegrcnt;
	u16 rcvegr_tid_base;
	u32 piocnt;
	u32 pio_base;
	u32 piobufs;
	u32 rcvegrbuf_chunks;
	u16 rcvegrbufs_perchunk;
	u16 rcvegrbufs_perchunk_shift;
	size_t rcvegrbuf_size;
	size_t rcvhdrq_size;
	unsigned long flag;
	u32 tidcursor;
	u32 rcvwait_to;
	u32 piowait_to;
	u32 rcvnowait;
	u32 pionowait;
	u32 urgent;
	u32 urgent_poll;
	pid_t pid;
	pid_t subpid[QLOGIC_IB_MAX_SUBCTXT];
	char comm[TASK_COMM_LEN];
	u16 pkeys[4];
	struct qib_devdata *dd;
	struct qib_pportdata *ppd;
	void *subctxt_uregbase;
	void *subctxt_rcvegrbuf;
	void *subctxt_rcvhdr_base;
	u32 userversion;
	u32 active_slaves;
	u16 poll_type;
	u8 seq_cnt;
	u8 redirect_seq_cnt;
	u32 head;
	struct list_head qp_wait_list;
#ifdef CONFIG_DEBUG_FS
	struct qib_opcode_stats_perctx *opstats;
#endif
};
struct rvt_sge_state;
struct qib_sdma_txreq {
	int                 flags;
	int                 sg_count;
	dma_addr_t          addr;
	void              (*callback)(struct qib_sdma_txreq *, int);
	u16                 start_idx;   
	u16                 next_descq_idx;   
	struct list_head    list;        
};
struct qib_sdma_desc {
	__le64 qw[2];
};
struct qib_verbs_txreq {
	struct qib_sdma_txreq   txreq;
	struct rvt_qp           *qp;
	struct rvt_swqe         *wqe;
	u32                     dwords;
	u16                     hdr_dwords;
	u16                     hdr_inx;
	struct qib_pio_header	*align_buf;
	struct rvt_mregion	*mr;
	struct rvt_sge_state    *ss;
};
#define QIB_SDMA_TXREQ_F_USELARGEBUF  0x1
#define QIB_SDMA_TXREQ_F_HEADTOHOST   0x2
#define QIB_SDMA_TXREQ_F_INTREQ       0x4
#define QIB_SDMA_TXREQ_F_FREEBUF      0x8
#define QIB_SDMA_TXREQ_F_FREEDESC     0x10
#define QIB_SDMA_TXREQ_S_OK        0
#define QIB_SDMA_TXREQ_S_SENDERROR 1
#define QIB_SDMA_TXREQ_S_ABORTED   2
#define QIB_SDMA_TXREQ_S_SHUTDOWN  3
#define QIB_IB_CFG_LIDLMC 0  
#define QIB_IB_CFG_LWID_ENB 2  
#define QIB_IB_CFG_LWID 3  
#define QIB_IB_CFG_SPD_ENB 4  
#define QIB_IB_CFG_SPD 5  
#define QIB_IB_CFG_RXPOL_ENB 6  
#define QIB_IB_CFG_LREV_ENB 7  
#define QIB_IB_CFG_LINKLATENCY 8  
#define QIB_IB_CFG_HRTBT 9  
#define QIB_IB_CFG_OP_VLS 10  
#define QIB_IB_CFG_VL_HIGH_CAP 11  
#define QIB_IB_CFG_VL_LOW_CAP 12  
#define QIB_IB_CFG_OVERRUN_THRESH 13  
#define QIB_IB_CFG_PHYERR_THRESH 14  
#define QIB_IB_CFG_LINKDEFAULT 15  
#define QIB_IB_CFG_PKEYS 16  
#define QIB_IB_CFG_MTU 17  
#define QIB_IB_CFG_LSTATE 18  
#define QIB_IB_CFG_VL_HIGH_LIMIT 19
#define QIB_IB_CFG_PMA_TICKS 20  
#define QIB_IB_CFG_PORT 21  
#define   IB_LINKCMD_DOWN   (0 << 16)
#define   IB_LINKCMD_ARMED  (1 << 16)
#define   IB_LINKCMD_ACTIVE (2 << 16)
#define   IB_LINKINITCMD_NOP     0
#define   IB_LINKINITCMD_POLL    1
#define   IB_LINKINITCMD_SLEEP   2
#define   IB_LINKINITCMD_DISABLE 3
#define QIB_IB_LINKDOWN         0
#define QIB_IB_LINKARM          1
#define QIB_IB_LINKACTIVE       2
#define QIB_IB_LINKDOWN_ONLY    3
#define QIB_IB_LINKDOWN_SLEEP   4
#define QIB_IB_LINKDOWN_DISABLE 5
#define QIB_IB_SDR 1
#define QIB_IB_DDR 2
#define QIB_IB_QDR 4
#define QIB_DEFAULT_MTU 4096
#define QIB_MAX_IB_PORTS 2
#define QIB_IB_TBL_VL_HIGH_ARB 1  
#define QIB_IB_TBL_VL_LOW_ARB 2  
#define QIB_RCVCTRL_TAILUPD_ENB 0x01
#define QIB_RCVCTRL_TAILUPD_DIS 0x02
#define QIB_RCVCTRL_CTXT_ENB 0x04
#define QIB_RCVCTRL_CTXT_DIS 0x08
#define QIB_RCVCTRL_INTRAVAIL_ENB 0x10
#define QIB_RCVCTRL_INTRAVAIL_DIS 0x20
#define QIB_RCVCTRL_PKEY_ENB 0x40   
#define QIB_RCVCTRL_PKEY_DIS 0x80
#define QIB_RCVCTRL_BP_ENB 0x0100
#define QIB_RCVCTRL_BP_DIS 0x0200
#define QIB_RCVCTRL_TIDFLOW_ENB 0x0400
#define QIB_RCVCTRL_TIDFLOW_DIS 0x0800
#define QIB_SENDCTRL_DISARM       (0x1000)
#define QIB_SENDCTRL_DISARM_BUF(bufn) ((bufn) | QIB_SENDCTRL_DISARM)
#define QIB_SENDCTRL_AVAIL_DIS    (0x4000)
#define QIB_SENDCTRL_AVAIL_ENB    (0x8000)
#define QIB_SENDCTRL_AVAIL_BLIP  (0x10000)
#define QIB_SENDCTRL_SEND_DIS    (0x20000)
#define QIB_SENDCTRL_SEND_ENB    (0x40000)
#define QIB_SENDCTRL_FLUSH       (0x80000)
#define QIB_SENDCTRL_CLEAR      (0x100000)
#define QIB_SENDCTRL_DISARM_ALL (0x200000)
#define QIBPORTCNTR_PKTSEND         0U
#define QIBPORTCNTR_WORDSEND        1U
#define QIBPORTCNTR_PSXMITDATA      2U
#define QIBPORTCNTR_PSXMITPKTS      3U
#define QIBPORTCNTR_PSXMITWAIT      4U
#define QIBPORTCNTR_SENDSTALL       5U
#define QIBPORTCNTR_PKTRCV          6U
#define QIBPORTCNTR_PSRCVDATA       7U
#define QIBPORTCNTR_PSRCVPKTS       8U
#define QIBPORTCNTR_RCVEBP          9U
#define QIBPORTCNTR_RCVOVFL         10U
#define QIBPORTCNTR_WORDRCV         11U
#define QIBPORTCNTR_RXLOCALPHYERR   12U
#define QIBPORTCNTR_RXVLERR         13U
#define QIBPORTCNTR_ERRICRC         14U
#define QIBPORTCNTR_ERRVCRC         15U
#define QIBPORTCNTR_ERRLPCRC        16U
#define QIBPORTCNTR_BADFORMAT       17U
#define QIBPORTCNTR_ERR_RLEN        18U
#define QIBPORTCNTR_IBSYMBOLERR     19U
#define QIBPORTCNTR_INVALIDRLEN     20U
#define QIBPORTCNTR_UNSUPVL         21U
#define QIBPORTCNTR_EXCESSBUFOVFL   22U
#define QIBPORTCNTR_ERRLINK         23U
#define QIBPORTCNTR_IBLINKDOWN      24U
#define QIBPORTCNTR_IBLINKERRRECOV  25U
#define QIBPORTCNTR_LLI             26U
#define QIBPORTCNTR_RXDROPPKT       27U
#define QIBPORTCNTR_VL15PKTDROP     28U
#define QIBPORTCNTR_ERRPKEY         29U
#define QIBPORTCNTR_KHDROVFL        30U
#define QIBPORTCNTR_PSINTERVAL      31U
#define QIBPORTCNTR_PSSTART         32U
#define QIBPORTCNTR_PSSTAT          33U
#define ACTIVITY_TIMER 5
#define MAX_NAME_SIZE 64
#ifdef CONFIG_INFINIBAND_QIB_DCA
struct qib_irq_notify;
#endif
struct qib_msix_entry {
	void *arg;
#ifdef CONFIG_INFINIBAND_QIB_DCA
	int dca;
	int rcv;
	struct qib_irq_notify *notifier;
#endif
	cpumask_var_t mask;
};
struct qib_chip_specific;
struct qib_chipport_specific;
enum qib_sdma_states {
	qib_sdma_state_s00_hw_down,
	qib_sdma_state_s10_hw_start_up_wait,
	qib_sdma_state_s20_idle,
	qib_sdma_state_s30_sw_clean_up_wait,
	qib_sdma_state_s40_hw_clean_up_wait,
	qib_sdma_state_s50_hw_halt_wait,
	qib_sdma_state_s99_running,
};
enum qib_sdma_events {
	qib_sdma_event_e00_go_hw_down,
	qib_sdma_event_e10_go_hw_start,
	qib_sdma_event_e20_hw_started,
	qib_sdma_event_e30_go_running,
	qib_sdma_event_e40_sw_cleaned,
	qib_sdma_event_e50_hw_cleaned,
	qib_sdma_event_e60_hw_halted,
	qib_sdma_event_e70_go_idle,
	qib_sdma_event_e7220_err_halted,
	qib_sdma_event_e7322_err_halted,
	qib_sdma_event_e90_timer_tick,
};
struct sdma_set_state_action {
	unsigned op_enable:1;
	unsigned op_intenable:1;
	unsigned op_halt:1;
	unsigned op_drain:1;
	unsigned go_s99_running_tofalse:1;
	unsigned go_s99_running_totrue:1;
};
struct qib_sdma_state {
	struct kref          kref;
	struct completion    comp;
	enum qib_sdma_states current_state;
	struct sdma_set_state_action *set_state_action;
	unsigned             current_op;
	unsigned             go_s99_running;
	unsigned             first_sendbuf;
	unsigned             last_sendbuf;  
	enum qib_sdma_states previous_state;
	unsigned             previous_op;
	enum qib_sdma_events last_event;
};
struct xmit_wait {
	struct timer_list timer;
	u64 counter;
	u8 flags;
	struct cache {
		u64 psxmitdata;
		u64 psrcvdata;
		u64 psxmitpkts;
		u64 psrcvpkts;
		u64 psxmitwait;
	} counter_cache;
};
struct qib_pportdata {
	struct qib_ibport ibport_data;
	struct qib_devdata *dd;
	struct qib_chippport_specific *cpspec;  
	__be64 guid;
	u32 lflags;
	u32 state_wanted;
	spinlock_t lflags_lock;
	atomic_t pkeyrefs[4];
	u64 *statusp;
	struct qib_sdma_desc *sdma_descq;
	struct workqueue_struct *qib_wq;
	struct qib_sdma_state sdma_state;
	dma_addr_t       sdma_descq_phys;
	volatile __le64 *sdma_head_dma;  
	dma_addr_t       sdma_head_phys;
	u16                   sdma_descq_cnt;
	spinlock_t            sdma_lock ____cacheline_aligned_in_smp;
	struct list_head      sdma_activelist;
	struct list_head      sdma_userpending;
	u64                   sdma_descq_added;
	u64                   sdma_descq_removed;
	u16                   sdma_descq_tail;
	u16                   sdma_descq_head;
	u8                    sdma_generation;
	u8                    sdma_intrequest;
	struct tasklet_struct sdma_sw_clean_up_task
		____cacheline_aligned_in_smp;
	wait_queue_head_t state_wait;  
	unsigned          hol_state;
	struct timer_list hol_timer;
	u64 lastibcstat;
	unsigned long p_rcvctrl;  
	unsigned long p_sendctrl;  
	u32 ibmtu;  
	u32 ibmaxlen;
	u32 init_ibmaxlen;
	u16 lid;
	u16 pkeys[4];
	u8 lmc;
	u8 link_width_supported;
	u16 link_speed_supported;
	u8 link_width_enabled;
	u16 link_speed_enabled;
	u8 link_width_active;
	u16 link_speed_active;
	u8 vls_supported;
	u8 vls_operational;
	u8 rx_pol_inv;
	u8 hw_pidx;      
	u32 port;         
	u8 delay_mult;
	u8 led_override;   
	u16 led_override_timeoff;  
	u8 led_override_vals[2];  
	u8 led_override_phase;  
	atomic_t led_override_timer_active;
	struct timer_list led_override_timer;
	struct xmit_wait cong_stats;
	struct timer_list symerr_clear_timer;
	spinlock_t cc_shadow_lock
		____cacheline_aligned_in_smp;
	struct cc_table_shadow *ccti_entries_shadow;
	struct ib_cc_congestion_setting_attr_shadow *congestion_entries_shadow;
	struct ib_cc_table_entry_shadow *ccti_entries;
	struct ib_cc_congestion_entry_shadow *congestion_entries;
	u16 cc_supported_table_entries;
	u16 total_cct_entry;
	u16 cc_sl_control_map;
	u16 ccti_limit;
	u8 cc_max_table_entries;
};
struct diag_observer;
typedef int (*diag_hook) (struct qib_devdata *dd,
	const struct diag_observer *op,
	u32 offs, u64 *data, u64 mask, int only_32);
struct diag_observer {
	diag_hook hook;
	u32 bottom;
	u32 top;
};
extern int qib_register_observer(struct qib_devdata *dd,
	const struct diag_observer *op);
struct diag_observer_list_elt;
struct qib_devdata {
	struct qib_ibdev verbs_dev;      
	struct list_head list;
	struct pci_dev *pcidev;
	struct cdev *user_cdev;
	struct cdev *diag_cdev;
	struct device *user_device;
	struct device *diag_device;
	u64 __iomem *kregbase;
	u64 __iomem *kregend;
	resource_size_t physaddr;
	struct qib_ctxtdata **rcd;  
	struct qib_pportdata *pport;
	struct qib_chip_specific *cspec;  
	void __iomem *pio2kbase;
	void __iomem *pio4kbase;
	void __iomem *piobase;
	u64 __iomem *userbase;
	void __iomem *piovl15base;  
	volatile __le64 *pioavailregs_dma;  
	dma_addr_t pioavailregs_phys;
	int (*f_intr_fallback)(struct qib_devdata *);
	int (*f_reset)(struct qib_devdata *);
	void (*f_quiet_serdes)(struct qib_pportdata *);
	int (*f_bringup_serdes)(struct qib_pportdata *);
	int (*f_early_init)(struct qib_devdata *);
	void (*f_clear_tids)(struct qib_devdata *, struct qib_ctxtdata *);
	void (*f_put_tid)(struct qib_devdata *, u64 __iomem*,
				u32, unsigned long);
	void (*f_cleanup)(struct qib_devdata *);
	void (*f_setextled)(struct qib_pportdata *, u32);
	int (*f_get_base_info)(struct qib_ctxtdata *, struct qib_base_info *);
	void (*f_free_irq)(struct qib_devdata *);
	struct qib_message_header *(*f_get_msgheader)
					(struct qib_devdata *, __le32 *);
	void (*f_config_ctxts)(struct qib_devdata *);
	int (*f_get_ib_cfg)(struct qib_pportdata *, int);
	int (*f_set_ib_cfg)(struct qib_pportdata *, int, u32);
	int (*f_set_ib_loopback)(struct qib_pportdata *, const char *);
	int (*f_get_ib_table)(struct qib_pportdata *, int, void *);
	int (*f_set_ib_table)(struct qib_pportdata *, int, void *);
	u32 (*f_iblink_state)(u64);
	u8 (*f_ibphys_portstate)(u64);
	void (*f_xgxs_reset)(struct qib_pportdata *);
	int (*f_ib_updown)(struct qib_pportdata *, int, u64);
	u32 __iomem *(*f_getsendbuf)(struct qib_pportdata *, u64, u32 *);
	int (*f_gpio_mod)(struct qib_devdata *dd, u32 out, u32 dir,
		u32 mask);
	int (*f_eeprom_wen)(struct qib_devdata *dd, int wen);
	void (*f_rcvctrl)(struct qib_pportdata *, unsigned int op,
		int ctxt);
	void (*f_sendctrl)(struct qib_pportdata *, u32 op);
	void (*f_set_intr_state)(struct qib_devdata *, u32);
	void (*f_set_armlaunch)(struct qib_devdata *, u32);
	void (*f_wantpiobuf_intr)(struct qib_devdata *, u32);
	int (*f_late_initreg)(struct qib_devdata *);
	int (*f_init_sdma_regs)(struct qib_pportdata *);
	u16 (*f_sdma_gethead)(struct qib_pportdata *);
	int (*f_sdma_busy)(struct qib_pportdata *);
	void (*f_sdma_update_tail)(struct qib_pportdata *, u16);
	void (*f_sdma_set_desc_cnt)(struct qib_pportdata *, unsigned);
	void (*f_sdma_sendctrl)(struct qib_pportdata *, unsigned);
	void (*f_sdma_hw_clean_up)(struct qib_pportdata *);
	void (*f_sdma_hw_start_up)(struct qib_pportdata *);
	void (*f_sdma_init_early)(struct qib_pportdata *);
	void (*f_set_cntr_sample)(struct qib_pportdata *, u32, u32);
	void (*f_update_usrhead)(struct qib_ctxtdata *, u64, u32, u32, u32);
	u32 (*f_hdrqempty)(struct qib_ctxtdata *);
	u64 (*f_portcntr)(struct qib_pportdata *, u32);
	u32 (*f_read_cntrs)(struct qib_devdata *, loff_t, char **,
		u64 **);
	u32 (*f_read_portcntrs)(struct qib_devdata *, loff_t, u32,
		char **, u64 **);
	u32 (*f_setpbc_control)(struct qib_pportdata *, u32, u8, u8);
	void (*f_initvl15_bufs)(struct qib_devdata *);
	void (*f_init_ctxt)(struct qib_ctxtdata *);
	void (*f_txchk_change)(struct qib_devdata *, u32, u32, u32,
		struct qib_ctxtdata *);
	void (*f_writescratch)(struct qib_devdata *, u32);
	int (*f_tempsense_rd)(struct qib_devdata *, int regnum);
#ifdef CONFIG_INFINIBAND_QIB_DCA
	int (*f_notify_dca)(struct qib_devdata *, unsigned long event);
#endif
	char *boardname;  
	u64 tidtemplate;
	u64 tidinvalid;
	u32 pioavregs;
	u32 flags;
	u32 lastctxt_piobuf;
	u64 z_int_counter;
	u64 __percpu *int_counter;
	u32 pbufsctxt;
	u32 ctxts_extrabuf;
	u32 cfgctxts;
	u32 freectxts;
	u32 upd_pio_shadow;
	u32 maxpkts_call;
	u32 avgpkts_call;
	u64 nopiobufs;
	u16 vendorid;
	u16 deviceid;
	int wc_cookie;
	unsigned long wc_base;
	unsigned long wc_len;
	struct page **pageshadow;
	dma_addr_t *physshadow;
	u64 __iomem *egrtidbase;
	spinlock_t sendctrl_lock;  
	spinlock_t uctxt_lock;  
	u64 *devstatusp;
	char *freezemsg;  
	u32 freezelen;  
	struct timer_list stats_timer;
	struct timer_list intrchk_timer;
	unsigned long ureg_align;  
	spinlock_t pioavail_lock;
	u32 last_pio;
	u32 min_kernel_pio;
	unsigned long pioavailshadow[6];
	unsigned long pioavailkernel[6];
	unsigned long pio_need_disarm[3];
	unsigned long pio_writing[3];
	u64 revision;
	__be64 base_guid;
	u64 piobufbase;
	u32 pio2k_bufbase;
	u32 nguid;
	unsigned long rcvctrl;  
	unsigned long sendctrl;  
	u32 rcvhdrcnt;
	u32 rcvhdrsize;
	u32 rcvhdrentsize;
	u32 ctxtcnt;
	u32 palign;
	u32 piobcnt2k;
	u32 piosize2k;
	u32 piosize2kmax_dwords;
	u32 piobcnt4k;
	u32 piosize4k;
	u32 rcvegrbase;
	u32 rcvtidbase;
	u32 rcvtidcnt;
	u32 uregbase;
	u32 control;
	u32 align4k;
	u16 rcvegrbufsize;
	u16 rcvegrbufsize_shift;
	u32 lbus_width;
	u32 lbus_speed;
	int unit;  
	u32 msi_lo;
	u32 msi_hi;
	u16 msi_data;
	u32 pcibar0;
	u32 pcibar1;
	u64 rhdrhead_intr_off;
	u8 serial[16];
	u8 boardversion[96];
	u8 lbus_info[32];  
	u8 majrev;
	u8 minrev;
	u8 num_pports;
	u8 first_user_ctxt;
	u8 n_krcv_queues;
	u8 qpn_mask;
	u8 skip_kctxt_mask;
	u16 rhf_offset;  
	u8 gpio_sda_num;
	u8 gpio_scl_num;
	u8 twsi_eeprom_dev;
	u8 board_atten;
	spinlock_t eep_st_lock;
	struct mutex eep_lock;
	uint64_t traffic_wds;
	struct qib_diag_client *diag_client;
	spinlock_t qib_diag_trans_lock;  
	struct diag_observer_list_elt *diag_observer_list;
	u8 psxmitwait_supported;
	u16 psxmitwait_check_rate;
	struct tasklet_struct error_tasklet;
	int assigned_node_id;  
};
#define QIB_HOL_UP       0
#define QIB_HOL_INIT     1
#define QIB_SDMA_SENDCTRL_OP_ENABLE    (1U << 0)
#define QIB_SDMA_SENDCTRL_OP_INTENABLE (1U << 1)
#define QIB_SDMA_SENDCTRL_OP_HALT      (1U << 2)
#define QIB_SDMA_SENDCTRL_OP_CLEANUP   (1U << 3)
#define QIB_SDMA_SENDCTRL_OP_DRAIN     (1U << 4)
#define TXCHK_CHG_TYPE_DIS1  3
#define TXCHK_CHG_TYPE_ENAB1 2
#define TXCHK_CHG_TYPE_KERN  1
#define TXCHK_CHG_TYPE_USER  0
#define QIB_CHASE_TIME msecs_to_jiffies(145)
#define QIB_CHASE_DIS_TIME msecs_to_jiffies(160)
struct qib_filedata {
	struct qib_ctxtdata *rcd;
	unsigned subctxt;
	unsigned tidcursor;
	struct qib_user_sdma_queue *pq;
	int rec_cpu_num;  
};
extern struct xarray qib_dev_table;
extern struct qib_devdata *qib_lookup(int unit);
extern u32 qib_cpulist_count;
extern unsigned long *qib_cpulist;
extern unsigned qib_cc_table_size;
int qib_init(struct qib_devdata *, int);
int init_chip_wc_pat(struct qib_devdata *dd, u32);
int qib_enable_wc(struct qib_devdata *dd);
void qib_disable_wc(struct qib_devdata *dd);
int qib_count_units(int *npresentp, int *nupp);
int qib_count_active_units(void);
int qib_cdev_init(int minor, const char *name,
		  const struct file_operations *fops,
		  struct cdev **cdevp, struct device **devp);
void qib_cdev_cleanup(struct cdev **cdevp, struct device **devp);
int qib_dev_init(void);
void qib_dev_cleanup(void);
int qib_diag_add(struct qib_devdata *);
void qib_diag_remove(struct qib_devdata *);
void qib_handle_e_ibstatuschanged(struct qib_pportdata *, u64);
void qib_sdma_update_tail(struct qib_pportdata *, u16);  
int qib_decode_err(struct qib_devdata *dd, char *buf, size_t blen, u64 err);
void qib_bad_intrstatus(struct qib_devdata *);
void qib_handle_urcv(struct qib_devdata *, u64);
void qib_chip_cleanup(struct qib_devdata *);
void qib_chip_done(void);
int qib_unordered_wc(void);
void qib_pio_copy(void __iomem *to, const void *from, size_t count);
void qib_disarm_piobufs(struct qib_devdata *, unsigned, unsigned);
int qib_disarm_piobufs_ifneeded(struct qib_ctxtdata *);
void qib_disarm_piobufs_set(struct qib_devdata *, unsigned long *, unsigned);
void qib_cancel_sends(struct qib_pportdata *);
int qib_create_rcvhdrq(struct qib_devdata *, struct qib_ctxtdata *);
int qib_setup_eagerbufs(struct qib_ctxtdata *);
void qib_set_ctxtcnt(struct qib_devdata *);
int qib_create_ctxts(struct qib_devdata *dd);
struct qib_ctxtdata *qib_create_ctxtdata(struct qib_pportdata *, u32, int);
int qib_init_pportdata(struct qib_pportdata *, struct qib_devdata *, u8, u8);
void qib_free_ctxtdata(struct qib_devdata *, struct qib_ctxtdata *);
u32 qib_kreceive(struct qib_ctxtdata *, u32 *, u32 *);
int qib_reset_device(int);
int qib_wait_linkstate(struct qib_pportdata *, u32, int);
int qib_set_linkstate(struct qib_pportdata *, u8);
int qib_set_mtu(struct qib_pportdata *, u16);
int qib_set_lid(struct qib_pportdata *, u32, u8);
void qib_hol_down(struct qib_pportdata *);
void qib_hol_init(struct qib_pportdata *);
void qib_hol_up(struct qib_pportdata *);
void qib_hol_event(struct timer_list *);
void qib_disable_after_error(struct qib_devdata *);
int qib_set_uevent_bits(struct qib_pportdata *, const int);
#define ctxt_fp(fp) \
	(((struct qib_filedata *)(fp)->private_data)->rcd)
#define subctxt_fp(fp) \
	(((struct qib_filedata *)(fp)->private_data)->subctxt)
#define tidcursor_fp(fp) \
	(((struct qib_filedata *)(fp)->private_data)->tidcursor)
#define user_sdma_queue_fp(fp) \
	(((struct qib_filedata *)(fp)->private_data)->pq)
static inline struct qib_devdata *dd_from_ppd(struct qib_pportdata *ppd)
{
	return ppd->dd;
}
static inline struct qib_devdata *dd_from_dev(struct qib_ibdev *dev)
{
	return container_of(dev, struct qib_devdata, verbs_dev);
}
static inline struct qib_devdata *dd_from_ibdev(struct ib_device *ibdev)
{
	return dd_from_dev(to_idev(ibdev));
}
static inline struct qib_pportdata *ppd_from_ibp(struct qib_ibport *ibp)
{
	return container_of(ibp, struct qib_pportdata, ibport_data);
}
static inline struct qib_ibport *to_iport(struct ib_device *ibdev, u32 port)
{
	struct qib_devdata *dd = dd_from_ibdev(ibdev);
	u32 pidx = port - 1;  
	WARN_ON(pidx >= dd->num_pports);
	return &dd->pport[pidx].ibport_data;
}
#define QIB_HAS_LINK_LATENCY  0x1  
#define QIB_INITTED           0x2  
#define QIB_DOING_RESET       0x4   
#define QIB_PRESENT           0x8   
#define QIB_PIO_FLUSH_WC      0x10  
#define QIB_HAS_THRESH_UPDATE 0x40
#define QIB_HAS_SDMA_TIMEOUT  0x80
#define QIB_USE_SPCL_TRIG     0x100  
#define QIB_NODMA_RTAIL       0x200  
#define QIB_HAS_INTX          0x800  
#define QIB_HAS_SEND_DMA      0x1000  
#define QIB_HAS_VLSUPP        0x2000  
#define QIB_HAS_HDRSUPP       0x4000  
#define QIB_BADINTR           0x8000  
#define QIB_DCA_ENABLED       0x10000  
#define QIB_HAS_QSFP          0x20000  
#define QIB_SHUTDOWN          0x40000  
#define QIBL_LINKV             0x1  
#define QIBL_LINKDOWN          0x8  
#define QIBL_LINKINIT          0x10  
#define QIBL_LINKARMED         0x20  
#define QIBL_LINKACTIVE        0x40  
#define QIBL_IB_AUTONEG_INPROG 0x1000  
#define QIBL_IB_AUTONEG_FAILED 0x2000  
#define QIBL_IB_LINK_DISABLED  0x4000  
#define QIBL_IB_FORCE_NOTIFY   0x8000  
#define QIB_PBC_LENGTH_MASK                     ((1 << 11) - 1)
#define QIB_CTXT_WAITING_RCV   2
#define QIB_CTXT_MASTER_UNINIT 4
#define QIB_CTXT_WAITING_URG 5
void qib_free_data(struct qib_ctxtdata *dd);
void qib_chg_pioavailkernel(struct qib_devdata *, unsigned, unsigned,
			    u32, struct qib_ctxtdata *);
struct qib_devdata *qib_init_iba7322_funcs(struct pci_dev *,
					   const struct pci_device_id *);
struct qib_devdata *qib_init_iba7220_funcs(struct pci_dev *,
					   const struct pci_device_id *);
struct qib_devdata *qib_init_iba6120_funcs(struct pci_dev *,
					   const struct pci_device_id *);
void qib_free_devdata(struct qib_devdata *);
struct qib_devdata *qib_alloc_devdata(struct pci_dev *pdev, size_t extra);
#define QIB_TWSI_NO_DEV 0xFF
int qib_twsi_reset(struct qib_devdata *dd);
int qib_twsi_blk_rd(struct qib_devdata *dd, int dev, int addr, void *buffer,
		    int len);
int qib_twsi_blk_wr(struct qib_devdata *dd, int dev, int addr,
		    const void *buffer, int len);
void qib_get_eeprom_info(struct qib_devdata *);
void qib_dump_lookup_output_queue(struct qib_devdata *);
void qib_force_pio_avail_update(struct qib_devdata *);
void qib_clear_symerror_on_linkup(struct timer_list *t);
#define QIB_LED_PHYS 1  
#define QIB_LED_LOG 2   
void qib_set_led_override(struct qib_pportdata *ppd, unsigned int val);
int qib_setup_sdma(struct qib_pportdata *);
void qib_teardown_sdma(struct qib_pportdata *);
void __qib_sdma_intr(struct qib_pportdata *);
void qib_sdma_intr(struct qib_pportdata *);
void qib_user_sdma_send_desc(struct qib_pportdata *dd,
			struct list_head *pktlist);
int qib_sdma_verbs_send(struct qib_pportdata *, struct rvt_sge_state *,
			u32, struct qib_verbs_txreq *);
int qib_sdma_make_progress(struct qib_pportdata *dd);
static inline u16 qib_sdma_descq_freecnt(const struct qib_pportdata *ppd)
{
	return ppd->sdma_descq_cnt -
		(ppd->sdma_descq_added - ppd->sdma_descq_removed) - 1;
}
static inline int __qib_sdma_running(struct qib_pportdata *ppd)
{
	return ppd->sdma_state.current_state == qib_sdma_state_s99_running;
}
int qib_sdma_running(struct qib_pportdata *);
void dump_sdma_state(struct qib_pportdata *ppd);
void __qib_sdma_process_event(struct qib_pportdata *, enum qib_sdma_events);
void qib_sdma_process_event(struct qib_pportdata *, enum qib_sdma_events);
#define QIB_DFLT_RCVHDRSIZE 9
#define QIB_RCVHDR_ENTSIZE 32
int qib_get_user_pages(unsigned long, size_t, struct page **);
void qib_release_user_pages(struct page **, size_t);
int qib_eeprom_read(struct qib_devdata *, u8, void *, int);
int qib_eeprom_write(struct qib_devdata *, u8, const void *, int);
u32 __iomem *qib_getsendbuf_range(struct qib_devdata *, u32 *, u32, u32);
void qib_sendbuf_done(struct qib_devdata *, unsigned);
static inline void qib_clear_rcvhdrtail(const struct qib_ctxtdata *rcd)
{
	*((u64 *) rcd->rcvhdrtail_kvaddr) = 0ULL;
}
static inline u32 qib_get_rcvhdrtail(const struct qib_ctxtdata *rcd)
{
	return (u32) le64_to_cpu(
		*((volatile __le64 *)rcd->rcvhdrtail_kvaddr));  
}
extern const char ib_qib_version[];
extern const struct attribute_group qib_attr_group;
extern const struct attribute_group *qib_attr_port_groups[];
int qib_device_create(struct qib_devdata *);
void qib_device_remove(struct qib_devdata *);
extern int qib_qsfp_dump(struct qib_pportdata *ppd, char *buf, int len);
int __init qib_init_qibfs(void);
int __exit qib_exit_qibfs(void);
int qibfs_add(struct qib_devdata *);
int qibfs_remove(struct qib_devdata *);
int qib_pcie_init(struct pci_dev *, const struct pci_device_id *);
int qib_pcie_ddinit(struct qib_devdata *, struct pci_dev *,
		    const struct pci_device_id *);
void qib_pcie_ddcleanup(struct qib_devdata *);
int qib_pcie_params(struct qib_devdata *dd, u32 minw, u32 *nent);
void qib_free_irq(struct qib_devdata *dd);
int qib_reinit_intr(struct qib_devdata *dd);
void qib_pcie_getcmd(struct qib_devdata *, u16 *, u8 *, u8 *);
void qib_pcie_reenable(struct qib_devdata *, u16, u8, u8);
u64 qib_int_counter(struct qib_devdata *);
u64 qib_sps_ints(void);
int qib_map_page(struct pci_dev *d, struct page *p, dma_addr_t *daddr);
struct pci_dev *qib_get_pci_dev(struct rvt_dev_info *rdi);
static inline void qib_flush_wc(void)
{
#if defined(CONFIG_X86_64)
	asm volatile("sfence" : : : "memory");
#else
	wmb();  
#endif
}
extern unsigned qib_ibmtu;
extern ushort qib_cfgctxts;
extern ushort qib_num_cfg_vls;
extern ushort qib_mini_init;  
extern unsigned qib_n_krcv_queues;
extern unsigned qib_sdma_fetch_arb;
extern unsigned qib_compat_ddr_negotiate;
extern int qib_special_trigger;
extern unsigned qib_numa_aware;
extern struct mutex qib_mutex;
#define STATUS_TIMEOUT 60
#define QIB_DRV_NAME            "ib_qib"
#define QIB_USER_MINOR_BASE     0
#define QIB_TRACE_MINOR         127
#define QIB_DIAGPKT_MINOR       128
#define QIB_DIAG_MINOR_BASE     129
#define QIB_NMINORS             255
#define PCI_VENDOR_ID_PATHSCALE 0x1fc1
#define PCI_VENDOR_ID_QLOGIC 0x1077
#define PCI_DEVICE_ID_QLOGIC_IB_6120 0x10
#define PCI_DEVICE_ID_QLOGIC_IB_7220 0x7220
#define PCI_DEVICE_ID_QLOGIC_IB_7322 0x7322
#define qib_early_err(dev, fmt, ...) \
	dev_err(dev, fmt, ##__VA_ARGS__)
#define qib_dev_err(dd, fmt, ...) \
	dev_err(&(dd)->pcidev->dev, "%s: " fmt, \
		rvt_get_ibdev_name(&(dd)->verbs_dev.rdi), ##__VA_ARGS__)
#define qib_dev_warn(dd, fmt, ...) \
	dev_warn(&(dd)->pcidev->dev, "%s: " fmt, \
		 rvt_get_ibdev_name(&(dd)->verbs_dev.rdi), ##__VA_ARGS__)
#define qib_dev_porterr(dd, port, fmt, ...) \
	dev_err(&(dd)->pcidev->dev, "%s: IB%u:%u " fmt, \
		rvt_get_ibdev_name(&(dd)->verbs_dev.rdi), (dd)->unit, (port), \
		##__VA_ARGS__)
#define qib_devinfo(pcidev, fmt, ...) \
	dev_info(&(pcidev)->dev, fmt, ##__VA_ARGS__)
struct qib_hwerror_msgs {
	u64 mask;
	const char *msg;
	size_t sz;
};
#define QLOGIC_IB_HWE_MSG(a, b) { .mask = a, .msg = b }
void qib_format_hwerrors(u64 hwerrs,
			 const struct qib_hwerror_msgs *hwerrmsgs,
			 size_t nhwerrmsgs, char *msg, size_t lmsg);
void qib_stop_send_queue(struct rvt_qp *qp);
void qib_quiesce_qp(struct rvt_qp *qp);
void qib_flush_qp_waiters(struct rvt_qp *qp);
int qib_mtu_to_path_mtu(u32 mtu);
u32 qib_mtu_from_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp, u32 pmtu);
void qib_notify_error_qp(struct rvt_qp *qp);
int qib_get_pmtu_from_attr(struct rvt_dev_info *rdi, struct rvt_qp *qp,
			   struct ib_qp_attr *attr);
#endif                           
