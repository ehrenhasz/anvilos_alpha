

#include <pthread.h>
#include <stdbool.h>

typedef pthread_mutex_t mp_thread_mutex_t;

void mp_thread_init(void);
void mp_thread_deinit(void);
void mp_thread_gc_others(void);



void mp_thread_unix_begin_atomic_section(void);
void mp_thread_unix_end_atomic_section(void);


#if defined(__APPLE__)
extern bool mp_thread_is_realtime_enabled;
void mp_thread_set_realtime(void);
#endif
