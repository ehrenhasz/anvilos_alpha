


#ifndef	_SYS_UNIQUE_H
#define	_SYS_UNIQUE_H

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	UNIQUE_BITS	56

void unique_init(void);
void unique_fini(void);


uint64_t unique_create(void);


uint64_t unique_insert(uint64_t value);


void unique_remove(uint64_t value);

#ifdef	__cplusplus
}
#endif

#endif 
