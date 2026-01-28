#ifndef FS_NFS_NFS4BLOCKLAYOUT_H
#define FS_NFS_NFS4BLOCKLAYOUT_H
#include <linux/device-mapper.h>
#include <linux/nfs_fs.h>
#include <linux/sunrpc/rpc_pipe_fs.h>
#include "../nfs4_fs.h"
#include "../pnfs.h"
#include "../netns.h"
#define PAGE_CACHE_SECTORS (PAGE_SIZE >> SECTOR_SHIFT)
#define PAGE_CACHE_SECTOR_SHIFT (PAGE_SHIFT - SECTOR_SHIFT)
#define SECTOR_SIZE (1 << SECTOR_SHIFT)
struct pnfs_block_dev;
#define PNFS_BLOCK_MAX_UUIDS	4
#define PNFS_BLOCK_MAX_DEVICES	64
#define PNFS_BLOCK_UUID_LEN	128
struct pnfs_block_volume {
	enum pnfs_block_volume_type	type;
	union {
		struct {
			int		len;
			int		nr_sigs;
			struct {
				u64		offset;
				u32		sig_len;
				u8		sig[PNFS_BLOCK_UUID_LEN];
			} sigs[PNFS_BLOCK_MAX_UUIDS];
		} simple;
		struct {
			u64		start;
			u64		len;
			u32		volume;
		} slice;
		struct {
			u32		volumes_count;
			u32		volumes[PNFS_BLOCK_MAX_DEVICES];
		} concat;
		struct {
			u64		chunk_size;
			u32		volumes_count;
			u32		volumes[PNFS_BLOCK_MAX_DEVICES];
		} stripe;
		struct {
			enum scsi_code_set		code_set;
			enum scsi_designator_type	designator_type;
			int				designator_len;
			u8				designator[256];
			u64				pr_key;
		} scsi;
	};
};
struct pnfs_block_dev_map {
	u64			start;
	u64			len;
	u64			disk_offset;
	struct block_device		*bdev;
};
struct pnfs_block_dev {
	struct nfs4_deviceid_node	node;
	u64				start;
	u64				len;
	u32				nr_children;
	struct pnfs_block_dev		*children;
	u64				chunk_size;
	struct block_device		*bdev;
	u64				disk_offset;
	u64				pr_key;
	bool				pr_registered;
	bool (*map)(struct pnfs_block_dev *dev, u64 offset,
			struct pnfs_block_dev_map *map);
};
struct pnfs_block_extent {
	union {
		struct rb_node	be_node;
		struct list_head be_list;
	};
	struct nfs4_deviceid_node *be_device;
	sector_t	be_f_offset;	 
	sector_t	be_length;	 
	sector_t	be_v_offset;	 
	enum pnfs_block_extent_state be_state;	 
#define EXTENT_WRITTEN		1
#define EXTENT_COMMITTING	2
	unsigned int	be_tag;
};
struct pnfs_block_layout {
	struct pnfs_layout_hdr	bl_layout;
	struct rb_root		bl_ext_rw;
	struct rb_root		bl_ext_ro;
	spinlock_t		bl_ext_lock;    
	bool			bl_scsi_layout;
	u64			bl_lwb;
};
static inline struct pnfs_block_layout *
BLK_LO2EXT(struct pnfs_layout_hdr *lo)
{
	return container_of(lo, struct pnfs_block_layout, bl_layout);
}
static inline struct pnfs_block_layout *
BLK_LSEG2EXT(struct pnfs_layout_segment *lseg)
{
	return BLK_LO2EXT(lseg->pls_layout);
}
struct bl_pipe_msg {
	struct rpc_pipe_msg msg;
	wait_queue_head_t *bl_wq;
};
struct bl_msg_hdr {
	u8  type;
	u16 totallen;  
};
#define BL_DEVICE_UMOUNT               0x0  
#define BL_DEVICE_MOUNT                0x1  
#define BL_DEVICE_REQUEST_INIT         0x0  
#define BL_DEVICE_REQUEST_PROC         0x1  
#define BL_DEVICE_REQUEST_ERR          0x2  
struct nfs4_deviceid_node *bl_alloc_deviceid_node(struct nfs_server *server,
		struct pnfs_device *pdev, gfp_t gfp_mask);
void bl_free_deviceid_node(struct nfs4_deviceid_node *d);
int ext_tree_insert(struct pnfs_block_layout *bl,
		struct pnfs_block_extent *new);
int ext_tree_remove(struct pnfs_block_layout *bl, bool rw, sector_t start,
		sector_t end);
int ext_tree_mark_written(struct pnfs_block_layout *bl, sector_t start,
		sector_t len, u64 lwb);
bool ext_tree_lookup(struct pnfs_block_layout *bl, sector_t isect,
		struct pnfs_block_extent *ret, bool rw);
int ext_tree_prepare_commit(struct nfs4_layoutcommit_args *arg);
void ext_tree_mark_committed(struct nfs4_layoutcommit_args *arg, int status);
dev_t bl_resolve_deviceid(struct nfs_server *server,
		struct pnfs_block_volume *b, gfp_t gfp_mask);
int __init bl_init_pipefs(void);
void bl_cleanup_pipefs(void);
#endif  
