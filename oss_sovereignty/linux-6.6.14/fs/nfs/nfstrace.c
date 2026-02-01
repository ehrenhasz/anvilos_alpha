
 
#include <linux/nfs_fs.h>
#include <linux/namei.h>
#include "internal.h"

#define CREATE_TRACE_POINTS
#include "nfstrace.h"

EXPORT_TRACEPOINT_SYMBOL_GPL(nfs_fsync_enter);
EXPORT_TRACEPOINT_SYMBOL_GPL(nfs_fsync_exit);
EXPORT_TRACEPOINT_SYMBOL_GPL(nfs_xdr_status);
EXPORT_TRACEPOINT_SYMBOL_GPL(nfs_xdr_bad_filehandle);
