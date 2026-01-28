#ifndef _SPL_TSD_H
#define	_SPL_TSD_H
#include <sys/types.h>
#define	TSD_HASH_TABLE_BITS_DEFAULT	9
#define	TSD_KEYS_MAX			32768
#define	DTOR_PID			(PID_MAX_LIMIT+1)
#define	PID_KEY				(TSD_KEYS_MAX+1)
typedef void (*dtor_func_t)(void *);
extern int tsd_set(uint_t, void *);
extern void *tsd_get(uint_t);
extern void *tsd_get_by_thread(uint_t, kthread_t *);
extern void tsd_create(uint_t *, dtor_func_t);
extern void tsd_destroy(uint_t *);
extern void tsd_exit(void);
int spl_tsd_init(void);
void spl_tsd_fini(void);
#endif  
