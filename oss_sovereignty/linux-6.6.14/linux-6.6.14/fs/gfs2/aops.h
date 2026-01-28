#ifndef __AOPS_DOT_H__
#define __AOPS_DOT_H__
#include "incore.h"
extern void adjust_fs_space(struct inode *inode);
extern void gfs2_trans_add_databufs(struct gfs2_inode *ip, struct folio *folio,
				    size_t from, size_t len);
#endif  
