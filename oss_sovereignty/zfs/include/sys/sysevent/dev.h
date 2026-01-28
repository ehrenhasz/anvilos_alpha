#ifndef	_SYS_SYSEVENT_DEV_H
#define	_SYS_SYSEVENT_DEV_H
#include <sys/sysevent/eventdefs.h>
#ifdef	__cplusplus
extern "C" {
#endif
#define	EV_VERSION		"version"
#define	DEV_PHYS_PATH		"phys_path"
#define	DEV_NAME		"dev_name"
#define	DEV_DRIVER_NAME		"driver_name"
#define	DEV_INSTANCE		"instance"
#define	DEV_PROP_PREFIX		"prop-"
#ifdef __linux__
#define	DEV_IDENTIFIER		"devid"
#define	DEV_PATH		"path"
#define	DEV_IS_PART		"is_slice"
#define	DEV_SIZE		"dev_size"
#define	DEV_PARENT_SIZE		"dev_parent_size"
#endif  
#define	EV_V1			1
#define	MAX_PROP_COUNT		100
#define	PROP_LEN_LIMIT		1024
#ifdef	__cplusplus
}
#endif
#endif  
