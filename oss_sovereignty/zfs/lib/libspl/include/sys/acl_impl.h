#ifndef _SYS_ACL_IMPL_H
#define	_SYS_ACL_IMPL_H
#ifdef	__cplusplus
extern "C" {
#endif
#define	ACL_IS_TRIVIAL	0x10000
#define	ACL_IS_DIR	0x20000
typedef enum acl_type {
	ACLENT_T = 0,
	ACE_T = 1
} acl_type_t;
struct acl_info {
	acl_type_t acl_type;		 
	int acl_cnt;			 
	int acl_entry_size;		 
	int acl_flags;			 
	void *acl_aclp;			 
};
#ifdef	__cplusplus
}
#endif
#endif  
