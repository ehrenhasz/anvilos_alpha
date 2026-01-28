


#ifndef	_GCM_IMPL_H
#define	_GCM_IMPL_H



#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>


typedef void		(*gcm_mul_f)(uint64_t *, uint64_t *, uint64_t *);
typedef boolean_t	(*gcm_will_work_f)(void);

#define	GCM_IMPL_NAME_MAX (16)

typedef struct gcm_impl_ops {
	gcm_mul_f mul;
	gcm_will_work_f is_supported;
	char name[GCM_IMPL_NAME_MAX];
} gcm_impl_ops_t;

extern const gcm_impl_ops_t gcm_generic_impl;
#if defined(__x86_64) && defined(HAVE_PCLMULQDQ)
extern const gcm_impl_ops_t gcm_pclmulqdq_impl;
#endif


void gcm_impl_init(void);


const struct gcm_impl_ops *gcm_impl_get_ops(void);

#ifdef	__cplusplus
}
#endif

#endif	
