
 

  

#include <linux/fs.h>
#include <linux/exportfs.h>
#include "cifsglob.h"
#include "cifs_debug.h"
#include "cifsfs.h"

#ifdef CONFIG_CIFS_NFSD_EXPORT
static struct dentry *cifs_get_parent(struct dentry *dentry)
{
	 
	cifs_dbg(FYI, "get parent for %p\n", dentry);
	return ERR_PTR(-EACCES);
}

const struct export_operations cifs_export_ops = {
	.get_parent = cifs_get_parent,
 
};

#endif  

