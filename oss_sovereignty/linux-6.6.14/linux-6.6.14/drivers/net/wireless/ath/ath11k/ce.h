#ifndef ATH11K_CE_H
#define ATH11K_CE_H
#define CE_COUNT_MAX 12
#define CE_ATTR_BYTE_SWAP_DATA 2
#define CE_ATTR_DIS_INTR		8
#ifdef __BIG_ENDIAN
#define CE_ATTR_FLAGS CE_ATTR_BYTE_SWAP_DATA
#else
#define CE_ATTR_FLAGS 0
#endif
#define ATH11K_CE_USAGE_THRESHOLD 32
void ath11k_ce_byte_swap(void *mem, u32 len);
#define PIPEDIR_NONE		0
#define PIPEDIR_IN		1  
#define PIPEDIR_OUT		2  
#define PIPEDIR_INOUT		3  
#define PIPEDIR_INOUT_H2H	4  
#define CE_HOST_IE_ADDRESS	0x00A1803C
#define CE_HOST_IE_2_ADDRESS	0x00A18040
#define CE_HOST_IE_3_ADDRESS	CE_HOST_IE_ADDRESS
#define CE_HOST_IPQ5018_IE_ADDRESS		0x0841804C
#define CE_HOST_IPQ5018_IE_2_ADDRESS		0x08418050
#define CE_HOST_IPQ5018_IE_3_ADDRESS		CE_HOST_IPQ5018_IE_ADDRESS
#define CE_HOST_IE_3_SHIFT	0xC
#define CE_RING_IDX_INCR(nentries_mask, idx) (((idx) + 1) & (nentries_mask))
#define ATH11K_CE_RX_POST_RETRY_JIFFIES 50
struct ath11k_base;
struct service_to_pipe {
	__le32 service_id;
	__le32 pipedir;
	__le32 pipenum;
};
struct ce_pipe_config {
	__le32 pipenum;
	__le32 pipedir;
	__le32 nentries;
	__le32 nbytes_max;
	__le32 flags;
	__le32 reserved;
};
struct ce_ie_addr {
	u32 ie1_reg_addr;
	u32 ie2_reg_addr;
	u32 ie3_reg_addr;
};
struct ce_remap {
	u32 base;
	u32 size;
};
struct ce_attr {
	unsigned int flags;
	unsigned int src_nentries;
	unsigned int src_sz_max;
	unsigned int dest_nentries;
	void (*recv_cb)(struct ath11k_base *, struct sk_buff *);
	void (*send_cb)(struct ath11k_base *, struct sk_buff *);
};
#define CE_DESC_RING_ALIGN 8
struct ath11k_ce_ring {
	unsigned int nentries;
	unsigned int nentries_mask;
	unsigned int sw_index;
	unsigned int write_index;
	void *base_addr_owner_space_unaligned;
	u32 base_addr_ce_space_unaligned;
	void *base_addr_owner_space;
	u32 base_addr_ce_space;
	u32 hal_ring_id;
	struct sk_buff *skb[];
};
struct ath11k_ce_pipe {
	struct ath11k_base *ab;
	u16 pipe_num;
	unsigned int attr_flags;
	unsigned int buf_sz;
	unsigned int rx_buf_needed;
	void (*send_cb)(struct ath11k_base *, struct sk_buff *);
	void (*recv_cb)(struct ath11k_base *, struct sk_buff *);
	struct tasklet_struct intr_tq;
	struct ath11k_ce_ring *src_ring;
	struct ath11k_ce_ring *dest_ring;
	struct ath11k_ce_ring *status_ring;
	u64 timestamp;
};
struct ath11k_ce {
	struct ath11k_ce_pipe ce_pipe[CE_COUNT_MAX];
	spinlock_t ce_lock;
	struct ath11k_hp_update_timer hp_timer[CE_COUNT_MAX];
};
extern const struct ce_attr ath11k_host_ce_config_ipq8074[];
extern const struct ce_attr ath11k_host_ce_config_qca6390[];
extern const struct ce_attr ath11k_host_ce_config_qcn9074[];
void ath11k_ce_cleanup_pipes(struct ath11k_base *ab);
void ath11k_ce_rx_replenish_retry(struct timer_list *t);
void ath11k_ce_per_engine_service(struct ath11k_base *ab, u16 ce_id);
int ath11k_ce_send(struct ath11k_base *ab, struct sk_buff *skb, u8 pipe_id,
		   u16 transfer_id);
void ath11k_ce_rx_post_buf(struct ath11k_base *ab);
int ath11k_ce_init_pipes(struct ath11k_base *ab);
int ath11k_ce_alloc_pipes(struct ath11k_base *ab);
void ath11k_ce_free_pipes(struct ath11k_base *ab);
int ath11k_ce_get_attr_flags(struct ath11k_base *ab, int ce_id);
void ath11k_ce_poll_send_completed(struct ath11k_base *ab, u8 pipe_id);
void ath11k_ce_get_shadow_config(struct ath11k_base *ab,
				 u32 **shadow_cfg, u32 *shadow_cfg_len);
void ath11k_ce_stop_shadow_timers(struct ath11k_base *ab);
#endif
