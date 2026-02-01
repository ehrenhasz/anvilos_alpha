 
 
#ifndef ZCRYPT_DEBUG_H
#define ZCRYPT_DEBUG_H

#include <asm/debug.h>

#define DBF_ERR		3	 
#define DBF_WARN	4	 
#define DBF_INFO	5	 
#define DBF_DEBUG	6	 

#define RC2ERR(rc) ((rc) ? DBF_ERR : DBF_INFO)
#define RC2WARN(rc) ((rc) ? DBF_WARN : DBF_INFO)

#define DBF_MAX_SPRINTF_ARGS 6

#define ZCRYPT_DBF(...)					\
	debug_sprintf_event(zcrypt_dbf_info, ##__VA_ARGS__)
#define ZCRYPT_DBF_ERR(...)					\
	debug_sprintf_event(zcrypt_dbf_info, DBF_ERR, ##__VA_ARGS__)
#define ZCRYPT_DBF_WARN(...)					\
	debug_sprintf_event(zcrypt_dbf_info, DBF_WARN, ##__VA_ARGS__)
#define ZCRYPT_DBF_INFO(...)					\
	debug_sprintf_event(zcrypt_dbf_info, DBF_INFO, ##__VA_ARGS__)
#define ZCRYPT_DBF_DBG(...)					\
	debug_sprintf_event(zcrypt_dbf_info, DBF_DEBUG, ##__VA_ARGS__)

extern debug_info_t *zcrypt_dbf_info;

int zcrypt_debug_init(void);
void zcrypt_debug_exit(void);

#endif  
