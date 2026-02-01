 

 
#ifndef _LIBSPL_LIBSHARE_H
#define	_LIBSPL_LIBSHARE_H extern __attribute__((visibility("default")))

#include <sys/types.h>

 
#define	SA_OK			0
#define	SA_SYSTEM_ERR		7	 
#define	SA_SYNTAX_ERR		8	 
#define	SA_NO_MEMORY		2	 
#define	SA_INVALID_PROTOCOL	13	 
#define	SA_NOT_SUPPORTED	21	 

 
#define	SA_NO_SUCH_PATH		1	 
#define	SA_DUPLICATE_NAME	3	 
#define	SA_BAD_PATH		4	 
#define	SA_NO_SUCH_GROUP	5	 
#define	SA_CONFIG_ERR		6	 
#define	SA_NO_PERMISSION	9	 
#define	SA_BUSY			10	 
#define	SA_NO_SUCH_PROP		11	 
#define	SA_INVALID_NAME		12	 
#define	SA_NOT_ALLOWED		14	 
#define	SA_BAD_VALUE		15	 
#define	SA_INVALID_SECURITY	16	 
#define	SA_NO_SUCH_SECURITY	17	 
#define	SA_VALUE_CONFLICT	18	 
#define	SA_NOT_IMPLEMENTED	19	 
#define	SA_INVALID_PATH		20	 
#define	SA_PROP_SHARE_ONLY	22	 
#define	SA_NOT_SHARED		23	 
#define	SA_NO_SUCH_RESOURCE	24	 
#define	SA_RESOURCE_REQUIRED	25	 
#define	SA_MULTIPLE_ERROR	26	 
#define	SA_PATH_IS_SUBDIR	27	 
#define	SA_PATH_IS_PARENTDIR	28	 
#define	SA_NO_SECTION		29	 
#define	SA_NO_SUCH_SECTION	30	 
#define	SA_NO_PROPERTIES	31	 
#define	SA_PASSWORD_ENC		32	 
#define	SA_SHARE_EXISTS		33	 

 
_LIBSPL_LIBSHARE_H const char *sa_errorstr(int);

 
enum sa_protocol {
	SA_PROTOCOL_NFS,
	SA_PROTOCOL_SMB,  
	SA_PROTOCOL_COUNT,
};

 
_LIBSPL_LIBSHARE_H const char *const sa_protocol_names[SA_PROTOCOL_COUNT];

 
_LIBSPL_LIBSHARE_H int sa_enable_share(const char *, const char *, const char *,
    enum sa_protocol);
_LIBSPL_LIBSHARE_H int sa_disable_share(const char *, enum sa_protocol);
_LIBSPL_LIBSHARE_H boolean_t sa_is_shared(const char *, enum sa_protocol);
_LIBSPL_LIBSHARE_H void sa_commit_shares(enum sa_protocol);
_LIBSPL_LIBSHARE_H void sa_truncate_shares(enum sa_protocol);

 
_LIBSPL_LIBSHARE_H int sa_validate_shareopts(const char *, enum sa_protocol);

#endif  
