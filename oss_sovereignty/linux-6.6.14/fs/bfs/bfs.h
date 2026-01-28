#ifndef _FS_BFS_BFS_H
#define _FS_BFS_BFS_H
#include <linux/bfs_fs.h>
#define BFS_MAX_LASTI	513
struct bfs_sb_info {
	unsigned long si_blocks;
	unsigned long si_freeb;
	unsigned long si_freei;
	unsigned long si_lf_eblk;
	unsigned long si_lasti;
	DECLARE_BITMAP(si_imap, BFS_MAX_LASTI+1);
	struct mutex bfs_lock;
};
struct bfs_inode_info {
	unsigned long i_dsk_ino;  
	unsigned long i_sblock;
	unsigned long i_eblock;
	struct inode vfs_inode;
};
static inline struct bfs_sb_info *BFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}
static inline struct bfs_inode_info *BFS_I(struct inode *inode)
{
	return container_of(inode, struct bfs_inode_info, vfs_inode);
}
#define printf(format, args...) \
	printk(KERN_ERR "BFS-fs: %s(): " format, __func__, ## args)
extern struct inode *bfs_iget(struct super_block *sb, unsigned long ino);
extern void bfs_dump_imap(const char *, struct super_block *);
extern const struct inode_operations bfs_file_inops;
extern const struct file_operations bfs_file_operations;
extern const struct address_space_operations bfs_aops;
extern const struct inode_operations bfs_dir_inops;
extern const struct file_operations bfs_dir_operations;
#endif  
