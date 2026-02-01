
 

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/nfs_ssc.h>
#include "../nfs/nfs4_fs.h"


struct nfs_ssc_client_ops_tbl nfs_ssc_client_tbl;
EXPORT_SYMBOL_GPL(nfs_ssc_client_tbl);

#ifdef CONFIG_NFS_V4_2
 
void nfs42_ssc_register(const struct nfs4_ssc_client_ops *ops)
{
	nfs_ssc_client_tbl.ssc_nfs4_ops = ops;
}
EXPORT_SYMBOL_GPL(nfs42_ssc_register);

 
void nfs42_ssc_unregister(const struct nfs4_ssc_client_ops *ops)
{
	if (nfs_ssc_client_tbl.ssc_nfs4_ops != ops)
		return;

	nfs_ssc_client_tbl.ssc_nfs4_ops = NULL;
}
EXPORT_SYMBOL_GPL(nfs42_ssc_unregister);
#endif  

#ifdef CONFIG_NFS_V4_2
 
void nfs_ssc_register(const struct nfs_ssc_client_ops *ops)
{
	nfs_ssc_client_tbl.ssc_nfs_ops = ops;
}
EXPORT_SYMBOL_GPL(nfs_ssc_register);

 
void nfs_ssc_unregister(const struct nfs_ssc_client_ops *ops)
{
	if (nfs_ssc_client_tbl.ssc_nfs_ops != ops)
		return;
	nfs_ssc_client_tbl.ssc_nfs_ops = NULL;
}
EXPORT_SYMBOL_GPL(nfs_ssc_unregister);

#else
void nfs_ssc_register(const struct nfs_ssc_client_ops *ops)
{
}
EXPORT_SYMBOL_GPL(nfs_ssc_register);

void nfs_ssc_unregister(const struct nfs_ssc_client_ops *ops)
{
}
EXPORT_SYMBOL_GPL(nfs_ssc_unregister);
#endif  
