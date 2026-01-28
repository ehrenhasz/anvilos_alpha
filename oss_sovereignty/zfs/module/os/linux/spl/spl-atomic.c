#include <sys/atomic.h>
#ifdef ATOMIC_SPINLOCK
DEFINE_SPINLOCK(atomic32_lock);
DEFINE_SPINLOCK(atomic64_lock);
EXPORT_SYMBOL(atomic32_lock);
EXPORT_SYMBOL(atomic64_lock);
#endif  
