


#ifndef	_ZINJECT_H
#define	_ZINJECT_H

#include <sys/zfs_ioctl.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
	TYPE_DATA,		
	TYPE_DNODE,		
	TYPE_MOS,		
	TYPE_MOSDIR,		
	TYPE_METASLAB,		
	TYPE_CONFIG,		
	TYPE_BPOBJ,		
	TYPE_SPACEMAP,		
	TYPE_ERRLOG,		
	TYPE_LABEL_UBERBLOCK,	
	TYPE_LABEL_NVLIST,	
	TYPE_LABEL_PAD1,	
	TYPE_LABEL_PAD2,	
	TYPE_INVAL
} err_type_t;

#define	MOS_TYPE(t)	\
	((t) >= TYPE_MOS && (t) < TYPE_LABEL_UBERBLOCK)

#define	LABEL_TYPE(t)	\
	((t) >= TYPE_LABEL_UBERBLOCK && (t) < TYPE_INVAL)

int translate_record(err_type_t type, const char *object, const char *range,
    int level, zinject_record_t *record, char *poolname, char *dataset);
int translate_raw(const char *raw, zinject_record_t *record);
int translate_device(const char *pool, const char *device,
    err_type_t label_type, zinject_record_t *record);
void usage(void);

extern libzfs_handle_t *g_zfs;

#ifdef	__cplusplus
}
#endif

#endif	
