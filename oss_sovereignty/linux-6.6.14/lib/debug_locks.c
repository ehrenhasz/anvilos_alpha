
 
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/debug_locks.h>

 
int debug_locks __read_mostly = 1;
EXPORT_SYMBOL_GPL(debug_locks);

 
int debug_locks_silent __read_mostly;
EXPORT_SYMBOL_GPL(debug_locks_silent);

 
int debug_locks_off(void)
{
	if (debug_locks && __debug_locks_off()) {
		if (!debug_locks_silent) {
			console_verbose();
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(debug_locks_off);
