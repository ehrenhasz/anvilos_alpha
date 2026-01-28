#ifndef LMAC_COMMON_H
#define LMAC_COMMON_H
#include "rvu.h"
#include "cgx.h"
struct lmac {
	wait_queue_head_t wq_cmd_cmplt;
	struct mutex cmd_lock;
	u64 resp;
	struct cgx_link_user_info link_info;
	struct rsrc_bmap mac_to_index_bmap;
	struct rsrc_bmap rx_fc_pfvf_bmap;
	struct rsrc_bmap tx_fc_pfvf_bmap;
	struct cgx_event_cb event_cb;
	spinlock_t event_cb_lock;
	struct cgx *cgx;
	u8 mcast_filters_count;
	u8 lmac_id;
	u8 lmac_type;
	bool cmd_pend;
	char *name;
};
struct mac_ops {
	char		       *name;
	u64			csr_offset;
	u64			int_register;
	u64			int_set_reg;
	u8			lmac_offset;
	u8			irq_offset;
	u8			int_ena_bit;
	u8			lmac_fwi;
	u32			fifo_len;
	bool			non_contiguous_serdes_lane;
	u8			rx_stats_cnt;
	u8			tx_stats_cnt;
	u64			rxid_map_offset;
	u8			dmac_filter_count;
	int			(*get_nr_lmacs)(void *cgx);
	u8                      (*get_lmac_type)(void *cgx, int lmac_id);
	u32                     (*lmac_fifo_len)(void *cgx, int lmac_id);
	int                     (*mac_lmac_intl_lbk)(void *cgx, int lmac_id,
						     bool enable);
	int			(*mac_get_rx_stats)(void *cgx, int lmac_id,
						    int idx, u64 *rx_stat);
	int			(*mac_get_tx_stats)(void *cgx, int lmac_id,
						    int idx, u64 *tx_stat);
	void			(*mac_enadis_rx_pause_fwding)(void *cgxd,
							      int lmac_id,
							      bool enable);
	int			(*mac_get_pause_frm_status)(void *cgxd,
							    int lmac_id,
							    u8 *tx_pause,
							    u8 *rx_pause);
	int			(*mac_enadis_pause_frm)(void *cgxd,
							int lmac_id,
							u8 tx_pause,
							u8 rx_pause);
	void			(*mac_pause_frm_config)(void  *cgxd,
							int lmac_id,
							bool enable);
	void			(*mac_enadis_ptp_config)(void  *cgxd,
							 int lmac_id,
							 bool enable);
	int			(*mac_rx_tx_enable)(void *cgxd, int lmac_id, bool enable);
	int			(*mac_tx_enable)(void *cgxd, int lmac_id, bool enable);
	int                     (*pfc_config)(void *cgxd, int lmac_id,
					      u8 tx_pause, u8 rx_pause, u16 pfc_en);
	int                     (*mac_get_pfc_frm_cfg)(void *cgxd, int lmac_id,
						       u8 *tx_pause, u8 *rx_pause);
	int			(*mac_reset)(void *cgxd, int lmac_id, u8 pf_req_flr);
	int			(*get_fec_stats)(void *cgxd, int lmac_id,
						 struct cgx_fec_stats_rsp *rsp);
};
struct cgx {
	void __iomem		*reg_base;
	struct pci_dev		*pdev;
	u8			cgx_id;
	u8			lmac_count;
	u8			max_lmac_per_mac;
#define MAX_LMAC_COUNT		8
	struct lmac             *lmac_idmap[MAX_LMAC_COUNT];
	struct			work_struct cgx_cmd_work;
	struct			workqueue_struct *cgx_cmd_workq;
	struct list_head	cgx_list;
	u64			hw_features;
	struct mac_ops		*mac_ops;
	unsigned long		lmac_bmap;  
	struct mutex		lock;
};
typedef struct cgx rpm_t;
void cgx_write(struct cgx *cgx, u64 lmac, u64 offset, u64 val);
u64 cgx_read(struct cgx *cgx, u64 lmac, u64 offset);
struct lmac *lmac_pdata(u8 lmac_id, struct cgx *cgx);
int cgx_fwi_cmd_send(u64 req, u64 *resp, struct lmac *lmac);
int cgx_fwi_cmd_generic(u64 req, u64 *resp, struct cgx *cgx, int lmac_id);
bool is_lmac_valid(struct cgx *cgx, int lmac_id);
struct mac_ops *rpm_get_mac_ops(struct cgx *cgx);
#endif  
