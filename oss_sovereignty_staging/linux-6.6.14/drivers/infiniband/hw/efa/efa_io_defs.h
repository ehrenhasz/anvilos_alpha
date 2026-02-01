 
 

#ifndef _EFA_IO_H_
#define _EFA_IO_H_

#define EFA_IO_TX_DESC_NUM_BUFS              2
#define EFA_IO_TX_DESC_NUM_RDMA_BUFS         1
#define EFA_IO_TX_DESC_INLINE_MAX_SIZE       32
#define EFA_IO_TX_DESC_IMM_DATA_SIZE         4

enum efa_io_queue_type {
	 
	EFA_IO_SEND_QUEUE                           = 1,
	 
	EFA_IO_RECV_QUEUE                           = 2,
};

enum efa_io_send_op_type {
	 
	EFA_IO_SEND                                 = 0,
	 
	EFA_IO_RDMA_READ                            = 1,
	 
	EFA_IO_RDMA_WRITE                           = 2,
};

enum efa_io_comp_status {
	 
	EFA_IO_COMP_STATUS_OK                       = 0,
	 
	EFA_IO_COMP_STATUS_FLUSHED                  = 1,
	 
	EFA_IO_COMP_STATUS_LOCAL_ERROR_QP_INTERNAL_ERROR = 2,
	 
	EFA_IO_COMP_STATUS_LOCAL_ERROR_INVALID_OP_TYPE = 3,
	 
	EFA_IO_COMP_STATUS_LOCAL_ERROR_INVALID_AH   = 4,
	 
	EFA_IO_COMP_STATUS_LOCAL_ERROR_INVALID_LKEY = 5,
	 
	EFA_IO_COMP_STATUS_LOCAL_ERROR_BAD_LENGTH   = 6,
	 
	EFA_IO_COMP_STATUS_REMOTE_ERROR_BAD_ADDRESS = 7,
	 
	EFA_IO_COMP_STATUS_REMOTE_ERROR_ABORT       = 8,
	 
	EFA_IO_COMP_STATUS_REMOTE_ERROR_BAD_DEST_QPN = 9,
	 
	EFA_IO_COMP_STATUS_REMOTE_ERROR_RNR         = 10,
	 
	EFA_IO_COMP_STATUS_REMOTE_ERROR_BAD_LENGTH  = 11,
	 
	EFA_IO_COMP_STATUS_REMOTE_ERROR_BAD_STATUS  = 12,
	 
	EFA_IO_COMP_STATUS_LOCAL_ERROR_UNRESP_REMOTE = 13,
};

struct efa_io_tx_meta_desc {
	 
	u16 req_id;

	 
	u8 ctrl1;

	 
	u8 ctrl2;

	u16 dest_qp_num;

	 
	u16 length;

	 
	u32 immediate_data;

	u16 ah;

	u16 reserved;

	 
	u32 qkey;

	u8 reserved2[12];
};

 
struct efa_io_tx_buf_desc {
	 
	u32 length;

	 
	u32 lkey;

	 
	u32 buf_addr_lo;

	 
	u32 buf_addr_hi;
};

struct efa_io_remote_mem_addr {
	 
	u32 length;

	 
	u32 rkey;

	 
	u32 buf_addr_lo;

	 
	u32 buf_addr_hi;
};

struct efa_io_rdma_req {
	 
	struct efa_io_remote_mem_addr remote_mem;

	 
	struct efa_io_tx_buf_desc local_mem[1];
};

 
struct efa_io_tx_wqe {
	 
	struct efa_io_tx_meta_desc meta;

	union {
		 
		struct efa_io_tx_buf_desc sgl[2];

		u8 inline_data[32];

		 
		struct efa_io_rdma_req rdma_req;
	} data;
};

 
struct efa_io_rx_desc {
	 
	u32 buf_addr_lo;

	 
	u32 buf_addr_hi;

	 
	u16 req_id;

	 
	u16 length;

	 
	u32 lkey_ctrl;
};

 
struct efa_io_cdesc_common {
	 
	u16 req_id;

	u8 status;

	 
	u8 flags;

	 
	u16 qp_num;
};

 
struct efa_io_tx_cdesc {
	 
	struct efa_io_cdesc_common common;

	 
	u16 reserved16;
};

 
struct efa_io_rx_cdesc {
	 
	struct efa_io_cdesc_common common;

	 
	u16 length;

	 
	u16 ah;

	u16 src_qp_num;

	 
	u32 imm;
};

 
struct efa_io_rx_cdesc_rdma_write {
	 
	u16 length_hi;
};

 
struct efa_io_rx_cdesc_ex {
	 
	struct efa_io_rx_cdesc base;

	union {
		struct efa_io_rx_cdesc_rdma_write rdma_write;

		 
		u8 src_addr[16];
	} u;
};

 
#define EFA_IO_TX_META_DESC_OP_TYPE_MASK                    GENMASK(3, 0)
#define EFA_IO_TX_META_DESC_HAS_IMM_MASK                    BIT(4)
#define EFA_IO_TX_META_DESC_INLINE_MSG_MASK                 BIT(5)
#define EFA_IO_TX_META_DESC_META_EXTENSION_MASK             BIT(6)
#define EFA_IO_TX_META_DESC_META_DESC_MASK                  BIT(7)
#define EFA_IO_TX_META_DESC_PHASE_MASK                      BIT(0)
#define EFA_IO_TX_META_DESC_FIRST_MASK                      BIT(2)
#define EFA_IO_TX_META_DESC_LAST_MASK                       BIT(3)
#define EFA_IO_TX_META_DESC_COMP_REQ_MASK                   BIT(4)

 
#define EFA_IO_TX_BUF_DESC_LKEY_MASK                        GENMASK(23, 0)

 
#define EFA_IO_RX_DESC_LKEY_MASK                            GENMASK(23, 0)
#define EFA_IO_RX_DESC_FIRST_MASK                           BIT(30)
#define EFA_IO_RX_DESC_LAST_MASK                            BIT(31)

 
#define EFA_IO_CDESC_COMMON_PHASE_MASK                      BIT(0)
#define EFA_IO_CDESC_COMMON_Q_TYPE_MASK                     GENMASK(2, 1)
#define EFA_IO_CDESC_COMMON_HAS_IMM_MASK                    BIT(3)
#define EFA_IO_CDESC_COMMON_OP_TYPE_MASK                    GENMASK(6, 4)

#endif  
