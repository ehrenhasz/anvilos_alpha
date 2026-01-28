#ifndef FS_NFS_NFS4FILELAYOUT_H
#define FS_NFS_NFS4FILELAYOUT_H
#include "../pnfs.h"
#define NFS4_PNFS_MAX_STRIPE_CNT 4096
#define NFS4_PNFS_MAX_MULTI_CNT  256  
enum stripetype4 {
	STRIPE_SPARSE = 1,
	STRIPE_DENSE = 2
};
struct nfs4_file_layout_dsaddr {
	struct nfs4_deviceid_node	id_node;
	u32				stripe_count;
	u8				*stripe_indices;
	u32				ds_num;
	struct nfs4_pnfs_ds		*ds_list[];
};
struct nfs4_filelayout_segment {
	struct pnfs_layout_segment	generic_hdr;
	u32				stripe_type;
	u32				commit_through_mds;
	u32				stripe_unit;
	u32				first_stripe_index;
	u64				pattern_offset;
	struct nfs4_deviceid		deviceid;
	struct nfs4_file_layout_dsaddr	*dsaddr;  
	unsigned int			num_fh;
	struct nfs_fh			**fh_array;
};
struct nfs4_filelayout {
	struct pnfs_layout_hdr generic_hdr;
	struct pnfs_ds_commit_info commit_info;
};
static inline struct nfs4_filelayout *
FILELAYOUT_FROM_HDR(struct pnfs_layout_hdr *lo)
{
	return container_of(lo, struct nfs4_filelayout, generic_hdr);
}
static inline struct nfs4_filelayout_segment *
FILELAYOUT_LSEG(struct pnfs_layout_segment *lseg)
{
	return container_of(lseg,
			    struct nfs4_filelayout_segment,
			    generic_hdr);
}
static inline struct nfs4_deviceid_node *
FILELAYOUT_DEVID_NODE(struct pnfs_layout_segment *lseg)
{
	return &FILELAYOUT_LSEG(lseg)->dsaddr->id_node;
}
static inline bool
filelayout_test_devid_invalid(struct nfs4_deviceid_node *node)
{
	return test_bit(NFS_DEVICEID_INVALID, &node->flags);
}
extern bool
filelayout_test_devid_unavailable(struct nfs4_deviceid_node *node);
extern struct nfs_fh *
nfs4_fl_select_ds_fh(struct pnfs_layout_segment *lseg, u32 j);
u32 nfs4_fl_calc_j_index(struct pnfs_layout_segment *lseg, loff_t offset);
u32 nfs4_fl_calc_ds_index(struct pnfs_layout_segment *lseg, u32 j);
struct nfs4_pnfs_ds *nfs4_fl_prepare_ds(struct pnfs_layout_segment *lseg,
					u32 ds_idx);
extern struct nfs4_file_layout_dsaddr *
nfs4_fl_alloc_deviceid_node(struct nfs_server *server,
	struct pnfs_device *pdev, gfp_t gfp_flags);
extern void nfs4_fl_put_deviceid(struct nfs4_file_layout_dsaddr *dsaddr);
extern void nfs4_fl_free_deviceid(struct nfs4_file_layout_dsaddr *dsaddr);
#endif  
