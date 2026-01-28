#ifndef _LINUX_NTFS_RUNLIST_H
#define _LINUX_NTFS_RUNLIST_H
#include "types.h"
#include "layout.h"
#include "volume.h"
typedef struct {	 
	VCN vcn;	 
	LCN lcn;	 
	s64 length;	 
} runlist_element;
typedef struct {
	runlist_element *rl;
	struct rw_semaphore lock;
} runlist;
static inline void ntfs_init_runlist(runlist *rl)
{
	rl->rl = NULL;
	init_rwsem(&rl->lock);
}
typedef enum {
	LCN_HOLE		= -1,	 
	LCN_RL_NOT_MAPPED	= -2,
	LCN_ENOENT		= -3,
	LCN_ENOMEM		= -4,
	LCN_EIO			= -5,
} LCN_SPECIAL_VALUES;
extern runlist_element *ntfs_runlists_merge(runlist_element *drl,
		runlist_element *srl);
extern runlist_element *ntfs_mapping_pairs_decompress(const ntfs_volume *vol,
		const ATTR_RECORD *attr, runlist_element *old_rl);
extern LCN ntfs_rl_vcn_to_lcn(const runlist_element *rl, const VCN vcn);
#ifdef NTFS_RW
extern runlist_element *ntfs_rl_find_vcn_nolock(runlist_element *rl,
		const VCN vcn);
extern int ntfs_get_size_for_mapping_pairs(const ntfs_volume *vol,
		const runlist_element *rl, const VCN first_vcn,
		const VCN last_vcn);
extern int ntfs_mapping_pairs_build(const ntfs_volume *vol, s8 *dst,
		const int dst_len, const runlist_element *rl,
		const VCN first_vcn, const VCN last_vcn, VCN *const stop_vcn);
extern int ntfs_rl_truncate_nolock(const ntfs_volume *vol,
		runlist *const runlist, const s64 new_length);
int ntfs_rl_punch_nolock(const ntfs_volume *vol, runlist *const runlist,
		const VCN start, const s64 length);
#endif  
#endif  
