#ifndef _CE_H_
#define _CE_H_
#include "hif.h"
#define CE_HTT_H2T_MSG_SRC_NENTRIES 8192
#define CE_DESC_RING_ALIGN	8
#define CE_SEND_FLAG_GATHER	0x00010000
struct ath10k_ce_pipe;
#define CE_DESC_FLAGS_GATHER         (1 << 0)
#define CE_DESC_FLAGS_BYTE_SWAP      (1 << 1)
#define CE_WCN3990_DESC_FLAGS_GATHER BIT(31)
#define CE_DESC_ADDR_MASK		GENMASK_ULL(34, 0)
#define CE_DESC_ADDR_HI_MASK		GENMASK(4, 0)
#define CE_DESC_FLAGS_HOST_INT_DIS	(1 << 2)
#define CE_DESC_FLAGS_TGT_INT_DIS	(1 << 3)
#define CE_DESC_FLAGS_META_DATA_MASK ar->hw_values->ce_desc_meta_data_mask
#define CE_DESC_FLAGS_META_DATA_LSB  ar->hw_values->ce_desc_meta_data_lsb
#define CE_DDR_RRI_MASK			GENMASK(15, 0)
#define CE_DDR_DRRI_SHIFT		16
struct ce_desc {
	__le32 addr;
	__le16 nbytes;
	__le16 flags;  
};
struct ce_desc_64 {
	__le64 addr;
	__le16 nbytes;  
	__le16 flags;  
	__le32 toeplitz_hash_result;
};
#define CE_DESC_SIZE sizeof(struct ce_desc)
#define CE_DESC_SIZE_64 sizeof(struct ce_desc_64)
struct ath10k_ce_ring {
	unsigned int nentries;
	unsigned int nentries_mask;
	unsigned int sw_index;
	unsigned int write_index;
	unsigned int hw_index;
	void *base_addr_owner_space_unaligned;
	dma_addr_t base_addr_ce_space_unaligned;
	void *base_addr_owner_space;
	dma_addr_t base_addr_ce_space;
	char *shadow_base_unaligned;
	struct ce_desc_64 *shadow_base;
	void *per_transfer_context[];
};
struct ath10k_ce_pipe {
	struct ath10k *ar;
	unsigned int id;
	unsigned int attr_flags;
	u32 ctrl_addr;
	void (*send_cb)(struct ath10k_ce_pipe *);
	void (*recv_cb)(struct ath10k_ce_pipe *);
	unsigned int src_sz_max;
	struct ath10k_ce_ring *src_ring;
	struct ath10k_ce_ring *dest_ring;
	const struct ath10k_ce_ops *ops;
};
struct ce_attr;
struct ath10k_bus_ops {
	u32 (*read32)(struct ath10k *ar, u32 offset);
	void (*write32)(struct ath10k *ar, u32 offset, u32 value);
	int (*get_num_banks)(struct ath10k *ar);
};
static inline struct ath10k_ce *ath10k_ce_priv(struct ath10k *ar)
{
	return (struct ath10k_ce *)ar->ce_priv;
}
struct ath10k_ce {
	spinlock_t ce_lock;
	const struct ath10k_bus_ops *bus_ops;
	struct ath10k_ce_pipe ce_states[CE_COUNT_MAX];
	u32 *vaddr_rri;
	dma_addr_t paddr_rri;
};
#define CE_SEND_FLAG_BYTE_SWAP 1
int ath10k_ce_send(struct ath10k_ce_pipe *ce_state,
		   void *per_transfer_send_context,
		   dma_addr_t buffer,
		   unsigned int nbytes,
		   unsigned int transfer_id,
		   unsigned int flags);
int ath10k_ce_send_nolock(struct ath10k_ce_pipe *ce_state,
			  void *per_transfer_context,
			  dma_addr_t buffer,
			  unsigned int nbytes,
			  unsigned int transfer_id,
			  unsigned int flags);
void __ath10k_ce_send_revert(struct ath10k_ce_pipe *pipe);
int ath10k_ce_num_free_src_entries(struct ath10k_ce_pipe *pipe);
int __ath10k_ce_rx_num_free_bufs(struct ath10k_ce_pipe *pipe);
int ath10k_ce_rx_post_buf(struct ath10k_ce_pipe *pipe, void *ctx,
			  dma_addr_t paddr);
void ath10k_ce_rx_update_write_idx(struct ath10k_ce_pipe *pipe, u32 nentries);
#define CE_RECV_FLAG_SWAPPED	1
int ath10k_ce_completed_recv_next(struct ath10k_ce_pipe *ce_state,
				  void **per_transfer_contextp,
				  unsigned int *nbytesp);
int ath10k_ce_completed_send_next(struct ath10k_ce_pipe *ce_state,
				  void **per_transfer_contextp);
int ath10k_ce_completed_send_next_nolock(struct ath10k_ce_pipe *ce_state,
					 void **per_transfer_contextp);
int ath10k_ce_init_pipe(struct ath10k *ar, unsigned int ce_id,
			const struct ce_attr *attr);
void ath10k_ce_deinit_pipe(struct ath10k *ar, unsigned int ce_id);
int ath10k_ce_alloc_pipe(struct ath10k *ar, int ce_id,
			 const struct ce_attr *attr);
void ath10k_ce_free_pipe(struct ath10k *ar, int ce_id);
int ath10k_ce_revoke_recv_next(struct ath10k_ce_pipe *ce_state,
			       void **per_transfer_contextp,
			       dma_addr_t *bufferp);
int ath10k_ce_completed_recv_next_nolock(struct ath10k_ce_pipe *ce_state,
					 void **per_transfer_contextp,
					 unsigned int *nbytesp);
int ath10k_ce_cancel_send_next(struct ath10k_ce_pipe *ce_state,
			       void **per_transfer_contextp,
			       dma_addr_t *bufferp,
			       unsigned int *nbytesp,
			       unsigned int *transfer_idp);
void ath10k_ce_per_engine_service_any(struct ath10k *ar);
void ath10k_ce_per_engine_service(struct ath10k *ar, unsigned int ce_id);
void ath10k_ce_disable_interrupt(struct ath10k *ar, int ce_id);
void ath10k_ce_disable_interrupts(struct ath10k *ar);
void ath10k_ce_enable_interrupt(struct ath10k *ar, int ce_id);
void ath10k_ce_enable_interrupts(struct ath10k *ar);
void ath10k_ce_dump_registers(struct ath10k *ar,
			      struct ath10k_fw_crash_data *crash_data);
void ath10k_ce_alloc_rri(struct ath10k *ar);
void ath10k_ce_free_rri(struct ath10k *ar);
#define CE_ATTR_NO_SNOOP		BIT(0)
#define CE_ATTR_BYTE_SWAP_DATA		BIT(1)
#define CE_ATTR_SWIZZLE_DESCRIPTORS	BIT(2)
#define CE_ATTR_DIS_INTR		BIT(3)
#define CE_ATTR_POLL			BIT(4)
struct ce_attr {
	unsigned int flags;
	unsigned int src_nentries;
	unsigned int src_sz_max;
	unsigned int dest_nentries;
	void (*send_cb)(struct ath10k_ce_pipe *);
	void (*recv_cb)(struct ath10k_ce_pipe *);
};
struct ath10k_ce_ops {
	struct ath10k_ce_ring *(*ce_alloc_src_ring)(struct ath10k *ar,
						    u32 ce_id,
						    const struct ce_attr *attr);
	struct ath10k_ce_ring *(*ce_alloc_dst_ring)(struct ath10k *ar,
						    u32 ce_id,
						    const struct ce_attr *attr);
	int (*ce_rx_post_buf)(struct ath10k_ce_pipe *pipe, void *ctx,
			      dma_addr_t paddr);
	int (*ce_completed_recv_next_nolock)(struct ath10k_ce_pipe *ce_state,
					     void **per_transfer_contextp,
					     u32 *nbytesp);
	int (*ce_revoke_recv_next)(struct ath10k_ce_pipe *ce_state,
				   void **per_transfer_contextp,
				   dma_addr_t *nbytesp);
	void (*ce_extract_desc_data)(struct ath10k *ar,
				     struct ath10k_ce_ring *src_ring,
				     u32 sw_index, dma_addr_t *bufferp,
				     u32 *nbytesp, u32 *transfer_idp);
	void (*ce_free_pipe)(struct ath10k *ar, int ce_id);
	int (*ce_send_nolock)(struct ath10k_ce_pipe *pipe,
			      void *per_transfer_context,
			      dma_addr_t buffer, u32 nbytes,
			      u32 transfer_id, u32 flags);
	void (*ce_set_src_ring_base_addr_hi)(struct ath10k *ar,
					     u32 ce_ctrl_addr,
					     u64 addr);
	void (*ce_set_dest_ring_base_addr_hi)(struct ath10k *ar,
					      u32 ce_ctrl_addr,
					      u64 addr);
	int (*ce_completed_send_next_nolock)(struct ath10k_ce_pipe *ce_state,
					     void **per_transfer_contextp);
};
static inline u32 ath10k_ce_base_address(struct ath10k *ar, unsigned int ce_id)
{
	return CE0_BASE_ADDRESS + (CE1_BASE_ADDRESS - CE0_BASE_ADDRESS) * ce_id;
}
#define COPY_ENGINE_ID(COPY_ENGINE_BASE_ADDRESS) (((COPY_ENGINE_BASE_ADDRESS) \
		- CE0_BASE_ADDRESS) / (CE1_BASE_ADDRESS - CE0_BASE_ADDRESS))
#define CE_SRC_RING_TO_DESC(baddr, idx) \
	(&(((struct ce_desc *)baddr)[idx]))
#define CE_DEST_RING_TO_DESC(baddr, idx) \
	(&(((struct ce_desc *)baddr)[idx]))
#define CE_SRC_RING_TO_DESC_64(baddr, idx) \
	(&(((struct ce_desc_64 *)baddr)[idx]))
#define CE_DEST_RING_TO_DESC_64(baddr, idx) \
	(&(((struct ce_desc_64 *)baddr)[idx]))
#define CE_RING_DELTA(nentries_mask, fromidx, toidx) \
	(((int)(toidx) - (int)(fromidx)) & (nentries_mask))
#define CE_RING_IDX_INCR(nentries_mask, idx) (((idx) + 1) & (nentries_mask))
#define CE_RING_IDX_ADD(nentries_mask, idx, num) \
		(((idx) + (num)) & (nentries_mask))
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB \
				ar->regs->ce_wrap_intr_sum_host_msi_lsb
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK \
				ar->regs->ce_wrap_intr_sum_host_msi_mask
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_GET(x) \
	(((x) & CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK) >> \
		CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB)
#define CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS			0x0000
static inline u32 ath10k_ce_interrupt_summary(struct ath10k *ar)
{
	struct ath10k_ce *ce = ath10k_ce_priv(ar);
	return CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_GET(
		ce->bus_ops->read32((ar), CE_WRAPPER_BASE_ADDRESS +
		CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS));
}
#define CE_ATTR_FLAGS 0
struct ce_pipe_config {
	__le32 pipenum;
	__le32 pipedir;
	__le32 nentries;
	__le32 nbytes_max;
	__le32 flags;
	__le32 reserved;
};
#define PIPEDIR_NONE    0
#define PIPEDIR_IN      1   
#define PIPEDIR_OUT     2   
#define PIPEDIR_INOUT   3   
struct ce_service_to_pipe {
	__le32 service_id;
	__le32 pipedir;
	__le32 pipenum;
};
#endif  
