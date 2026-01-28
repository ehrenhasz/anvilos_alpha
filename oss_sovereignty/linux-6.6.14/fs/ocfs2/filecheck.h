#ifndef FILECHECK_H
#define FILECHECK_H
#include <linux/types.h>
#include <linux/list.h>
enum {
	OCFS2_FILECHECK_ERR_SUCCESS = 0,	 
	OCFS2_FILECHECK_ERR_FAILED = 1000,	 
	OCFS2_FILECHECK_ERR_INPROGRESS,		 
	OCFS2_FILECHECK_ERR_READONLY,		 
	OCFS2_FILECHECK_ERR_INJBD,		 
	OCFS2_FILECHECK_ERR_INVALIDINO,		 
	OCFS2_FILECHECK_ERR_BLOCKECC,		 
	OCFS2_FILECHECK_ERR_BLOCKNO,		 
	OCFS2_FILECHECK_ERR_VALIDFLAG,		 
	OCFS2_FILECHECK_ERR_GENERATION,		 
	OCFS2_FILECHECK_ERR_UNSUPPORTED		 
};
#define OCFS2_FILECHECK_ERR_START	OCFS2_FILECHECK_ERR_FAILED
#define OCFS2_FILECHECK_ERR_END		OCFS2_FILECHECK_ERR_UNSUPPORTED
struct ocfs2_filecheck {
	struct list_head fc_head;	 
	spinlock_t fc_lock;
	unsigned int fc_max;	 
	unsigned int fc_size;	 
	unsigned int fc_done;	 
};
#define OCFS2_FILECHECK_MAXSIZE		100
#define OCFS2_FILECHECK_MINSIZE		10
enum {
	OCFS2_FILECHECK_TYPE_CHK = 0,	 
	OCFS2_FILECHECK_TYPE_FIX,	 
	OCFS2_FILECHECK_TYPE_SET = 100	 
};
struct ocfs2_filecheck_sysfs_entry {	 
	struct kobject fs_kobj;
	struct completion fs_kobj_unregister;
	struct ocfs2_filecheck *fs_fcheck;
};
int ocfs2_filecheck_create_sysfs(struct ocfs2_super *osb);
void ocfs2_filecheck_remove_sysfs(struct ocfs2_super *osb);
#endif   
