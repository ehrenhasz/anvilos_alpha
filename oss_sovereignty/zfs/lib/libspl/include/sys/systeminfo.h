#ifndef _LIBSPL_SYS_SYSTEMINFO_H
#define	_LIBSPL_SYS_SYSTEMINFO_H
#define	HOSTID_MASK		0xFFFFFFFF
#define	HW_INVALID_HOSTID	0xFFFFFFFF	 
#define	HW_HOSTID_LEN		11		 
unsigned long get_system_hostid(void);
#endif
