


#ifndef __NFP_ABI__
#define __NFP_ABI__ 1

#include <linux/types.h>

#define NFP_MBOX_SYM_NAME		"_abi_nfd_pf%u_mbox"
#define NFP_MBOX_SYM_MIN_SIZE		16 

#define NFP_MBOX_CMD		0x00
#define NFP_MBOX_RET		0x04
#define NFP_MBOX_DATA_LEN	0x08
#define NFP_MBOX_RESERVED	0x0c
#define NFP_MBOX_DATA		0x10


enum nfp_mbox_cmd {
	NFP_MBOX_NO_CMD			= 0x00,

	NFP_MBOX_POOL_GET		= 0x01,
	NFP_MBOX_POOL_SET		= 0x02,

	NFP_MBOX_PCIE_ABM_ENABLE	= 0x03,
	NFP_MBOX_PCIE_ABM_DISABLE	= 0x04,
};

#define NFP_SHARED_BUF_COUNT_SYM_NAME	"_abi_nfd_pf%u_sb_cnt"
#define NFP_SHARED_BUF_TABLE_SYM_NAME	"_abi_nfd_pf%u_sb_tbl"


struct nfp_shared_buf {
	__le32 id;
	__le32 size;
	__le16 ingress_pools_count;
	__le16 egress_pools_count;
	__le16 ingress_tc_count;
	__le16 egress_tc_count;

	__le32 pool_size_unit;
};


struct nfp_shared_buf_pool_id {
	__le32 shared_buf;
	__le32 pool;
};


struct nfp_shared_buf_pool_info_get {
	__le32 pool_type;
	__le32 size;
	__le32 threshold_type;
};


struct nfp_shared_buf_pool_info_set {
	struct nfp_shared_buf_pool_id id;
	__le32 size;
	__le32 threshold_type;
};

#endif
