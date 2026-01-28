#ifndef _QED_LL2_H
#define _QED_LL2_H
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/qed/qed_chain.h>
#include <linux/qed/qed_ll2_if.h>
#include "qed.h"
#include "qed_hsi.h"
#include "qed_sp.h"
#define QED_MAX_NUM_OF_LL2_CONNECTIONS                    (4)
#define QED_MAX_NUM_OF_LL2_CONNS_PF            (4)
#define QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF   (3)
#define QED_MAX_NUM_OF_CTX_LL2_CONNS_PF	\
	(QED_MAX_NUM_OF_LL2_CONNS_PF - QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF)
#define QED_LL2_LEGACY_CONN_BASE_PF     0
#define QED_LL2_CTX_CONN_BASE_PF        QED_MAX_NUM_OF_LEGACY_LL2_CONNS_PF
struct qed_ll2_rx_packet {
	struct list_head list_entry;
	struct core_rx_bd_with_buff_len *rxq_bd;
	dma_addr_t rx_buf_addr;
	u16 buf_length;
	void *cookie;
	u8 placement_offset;
	u16 parse_flags;
	u16 packet_length;
	u16 vlan;
	u32 opaque_data[2];
};
struct qed_ll2_tx_packet {
	struct list_head list_entry;
	u16 bd_used;
	bool notify_fw;
	void *cookie;
	struct {
		struct core_tx_bd *txq_bd;
		dma_addr_t tx_frag;
		u16 frag_len;
	} bds_set[];
};
struct qed_ll2_rx_queue {
	spinlock_t lock;
	struct qed_chain rxq_chain;
	struct qed_chain rcq_chain;
	u8 rx_sb_index;
	u8 ctx_based;
	bool b_cb_registered;
	__le16 *p_fw_cons;
	struct list_head active_descq;
	struct list_head free_descq;
	struct list_head posting_descq;
	struct qed_ll2_rx_packet *descq_array;
	void __iomem *set_prod_addr;
	struct core_pwm_prod_update_data db_data;
};
struct qed_ll2_tx_queue {
	spinlock_t lock;
	struct qed_chain txq_chain;
	u8 tx_sb_index;
	bool b_cb_registered;
	__le16 *p_fw_cons;
	struct list_head active_descq;
	struct list_head free_descq;
	struct list_head sending_descq;
	u16 cur_completing_bd_idx;
	void __iomem *doorbell_addr;
	struct core_db_data db_msg;
	u16 bds_idx;
	u16 cur_send_frag_num;
	u16 cur_completing_frag_num;
	bool b_completing_packet;
	void *descq_mem;  
	struct qed_ll2_tx_packet *cur_send_packet;
	struct qed_ll2_tx_packet cur_completing_packet;
};
struct qed_ll2_info {
	struct mutex mutex;
	struct qed_ll2_acquire_data_inputs input;
	u32 cid;
	u8 my_id;
	u8 queue_id;
	u8 tx_stats_id;
	bool b_active;
	enum core_tx_dest tx_dest;
	u8 tx_stats_en;
	bool main_func_queue;
	struct qed_ll2_cbs cbs;
	struct qed_ll2_rx_queue rx_queue;
	struct qed_ll2_tx_queue tx_queue;
};
extern const struct qed_ll2_ops qed_ll2_ops_pass;
int qed_ll2_acquire_connection(void *cxt, struct qed_ll2_acquire_data *data);
int qed_ll2_establish_connection(void *cxt, u8 connection_handle);
int qed_ll2_post_rx_buffer(void *cxt,
			   u8 connection_handle,
			   dma_addr_t addr,
			   u16 buf_len, void *cookie, u8 notify_fw);
int qed_ll2_prepare_tx_packet(void *cxt,
			      u8 connection_handle,
			      struct qed_ll2_tx_pkt_info *pkt,
			      bool notify_fw);
void qed_ll2_release_connection(void *cxt, u8 connection_handle);
int qed_ll2_set_fragment_of_tx_packet(void *cxt,
				      u8 connection_handle,
				      dma_addr_t addr, u16 nbytes);
int qed_ll2_terminate_connection(void *cxt, u8 connection_handle);
int qed_ll2_get_stats(void *cxt,
		      u8 connection_handle, struct qed_ll2_stats *p_stats);
int qed_ll2_alloc(struct qed_hwfn *p_hwfn);
void qed_ll2_setup(struct qed_hwfn *p_hwfn);
void qed_ll2_free(struct qed_hwfn *p_hwfn);
#endif
