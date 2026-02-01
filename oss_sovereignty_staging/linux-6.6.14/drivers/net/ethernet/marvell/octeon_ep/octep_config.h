 
 

#ifndef _OCTEP_CONFIG_H_
#define _OCTEP_CONFIG_H_

 
#define OCTEP_32BYTE_INSTR  32
#define OCTEP_64BYTE_INSTR  64

 
#define OCTEP_IQ_MAX_DESCRIPTORS    1024
 
#define OCTEP_DB_MIN                1
 
#define OCTEP_IQ_INTR_THRESHOLD     0x0

 
#define OCTEP_OQ_MAX_DESCRIPTORS   1024

 
#define OCTEP_OQ_BUF_SIZE          (SKB_WITH_OVERHEAD(PAGE_SIZE))
#define OCTEP_OQ_PKTS_PER_INTR     128
#define OCTEP_OQ_REFILL_THRESHOLD  (OCTEP_OQ_MAX_DESCRIPTORS / 4)

#define OCTEP_OQ_INTR_PKT_THRESHOLD   1
#define OCTEP_OQ_INTR_TIME_THRESHOLD  10

#define OCTEP_MSIX_NAME_SIZE      (IFNAMSIZ + 32)

 
#define OCTEP_WAKE_QUEUE_THRESHOLD 2

 
#define OCTEP_MIN_MTU        ETH_MIN_MTU
 
#define OCTEP_MAX_MTU        (10000 - (ETH_HLEN + ETH_FCS_LEN))
 
#define OCTEP_DEFAULT_MTU    1500

 
#define CFG_GET_IQ_CFG(cfg)             ((cfg)->iq)
#define CFG_GET_IQ_NUM_DESC(cfg)        ((cfg)->iq.num_descs)
#define CFG_GET_IQ_INSTR_TYPE(cfg)      ((cfg)->iq.instr_type)
#define CFG_GET_IQ_PKIND(cfg)           ((cfg)->iq.pkind)
#define CFG_GET_IQ_INSTR_SIZE(cfg)      (64)
#define CFG_GET_IQ_DB_MIN(cfg)          ((cfg)->iq.db_min)
#define CFG_GET_IQ_INTR_THRESHOLD(cfg)  ((cfg)->iq.intr_threshold)

#define CFG_GET_OQ_NUM_DESC(cfg)          ((cfg)->oq.num_descs)
#define CFG_GET_OQ_BUF_SIZE(cfg)          ((cfg)->oq.buf_size)
#define CFG_GET_OQ_REFILL_THRESHOLD(cfg)  ((cfg)->oq.refill_threshold)
#define CFG_GET_OQ_INTR_PKT(cfg)          ((cfg)->oq.oq_intr_pkt)
#define CFG_GET_OQ_INTR_TIME(cfg)         ((cfg)->oq.oq_intr_time)

#define CFG_GET_PORTS_MAX_IO_RINGS(cfg)    ((cfg)->pf_ring_cfg.max_io_rings)
#define CFG_GET_PORTS_ACTIVE_IO_RINGS(cfg) ((cfg)->pf_ring_cfg.active_io_rings)
#define CFG_GET_PORTS_PF_SRN(cfg)          ((cfg)->pf_ring_cfg.srn)

#define CFG_GET_DPI_PKIND(cfg)            ((cfg)->core_cfg.dpi_pkind)
#define CFG_GET_CORE_TICS_PER_US(cfg)     ((cfg)->core_cfg.core_tics_per_us)
#define CFG_GET_COPROC_TICS_PER_US(cfg)   ((cfg)->core_cfg.coproc_tics_per_us)

#define CFG_GET_MAX_VFS(cfg)        ((cfg)->sriov_cfg.max_vfs)
#define CFG_GET_ACTIVE_VFS(cfg)     ((cfg)->sriov_cfg.active_vfs)
#define CFG_GET_MAX_RPVF(cfg)       ((cfg)->sriov_cfg.max_rings_per_vf)
#define CFG_GET_ACTIVE_RPVF(cfg)    ((cfg)->sriov_cfg.active_rings_per_vf)
#define CFG_GET_VF_SRN(cfg)         ((cfg)->sriov_cfg.vf_srn)

#define CFG_GET_IOQ_MSIX(cfg)            ((cfg)->msix_cfg.ioq_msix)
#define CFG_GET_NON_IOQ_MSIX(cfg)        ((cfg)->msix_cfg.non_ioq_msix)
#define CFG_GET_NON_IOQ_MSIX_NAMES(cfg)  ((cfg)->msix_cfg.non_ioq_msix_names)

#define CFG_GET_CTRL_MBOX_MEM_ADDR(cfg)  ((cfg)->ctrl_mbox_cfg.barmem_addr)

 
struct octep_iq_config {
	 
	u16 num_descs;

	 
	u16 instr_type;

	 
	u16 pkind;

	 
	u16 db_min;

	 
	u32 intr_threshold;
};

 
struct octep_oq_config {
	 
	u16 num_descs;

	 
	u16 buf_size;

	 
	u16 refill_threshold;

	 
	u32 oq_intr_pkt;

	 
	u32 oq_intr_time;
};

 
struct octep_pf_ring_config {
	 
	u16 max_io_rings;

	 
	u16 active_io_rings;

	 
	u16 srn;
};

 
struct octep_sriov_config {
	 
	u16 max_vfs;

	 
	u16 active_vfs;

	 
	u8 max_rings_per_vf;

	 
	u8 active_rings_per_vf;

	 
	u16 vf_srn;
};

 
struct octep_msix_config {
	 
	u16 ioq_msix;

	 
	u16 non_ioq_msix;

	 
	char **non_ioq_msix_names;
};

struct octep_ctrl_mbox_config {
	 
	void __iomem *barmem_addr;
};

 
struct octep_config {
	 
	struct octep_iq_config iq;

	 
	struct octep_oq_config oq;

	 
	struct octep_pf_ring_config pf_ring_cfg;

	 
	struct octep_sriov_config sriov_cfg;

	 
	struct octep_msix_config msix_cfg;

	 
	struct octep_ctrl_mbox_config ctrl_mbox_cfg;

	 
	u32 max_hb_miss_cnt;

	 
	u32 hb_interval;
};
#endif  
