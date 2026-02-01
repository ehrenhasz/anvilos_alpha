 

#ifndef _SPL_ZFS_H
#define	_SPL_ZFS_H

#include <linux/mount.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/statfs.h>
#include <linux/xattr.h>
#include <linux/security.h>
#include <linux/seq_file.h>

#define	MAXFIDSZ	64

typedef struct spl_fid {
	union {
		long fid_pad;
		struct {
			ushort_t len;		 
			char data[MAXFIDSZ];	 
		} _fid;
	} un;
} fid_t;

#define	fid_len		un._fid.len
#define	fid_data	un._fid.data

#endif  
