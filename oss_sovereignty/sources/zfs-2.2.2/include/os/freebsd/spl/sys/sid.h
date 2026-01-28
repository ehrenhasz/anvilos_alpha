

#ifndef _OPENSOLARIS_SYS_SID_H_
#define	_OPENSOLARIS_SYS_SID_H_
#include <sys/idmap.h>

typedef struct ksiddomain {
	char	*kd_name;	
	uint_t	kd_len;
} ksiddomain_t;
typedef void	ksid_t;

static __inline ksiddomain_t *
ksid_lookupdomain(const char *domain)
{
	ksiddomain_t *kd;
	size_t len;

	len = strlen(domain) + 1;
	kd = kmem_alloc(sizeof (*kd), KM_SLEEP);
	kd->kd_len = (uint_t)len;
	kd->kd_name = kmem_alloc(len, KM_SLEEP);
	strcpy(kd->kd_name, domain);
	return (kd);
}

static __inline void
ksiddomain_rele(ksiddomain_t *kd)
{

	kmem_free(kd->kd_name, kd->kd_len);
	kmem_free(kd, sizeof (*kd));
}

#endif	
