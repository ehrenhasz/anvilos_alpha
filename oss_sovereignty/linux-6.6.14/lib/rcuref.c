

 

#include <linux/export.h>
#include <linux/rcuref.h>

 
bool rcuref_get_slowpath(rcuref_t *ref)
{
	unsigned int cnt = atomic_read(&ref->refcnt);

	 
	if (cnt >= RCUREF_RELEASED) {
		atomic_set(&ref->refcnt, RCUREF_DEAD);
		return false;
	}

	 
	if (WARN_ONCE(cnt > RCUREF_MAXREF, "rcuref saturated - leaking memory"))
		atomic_set(&ref->refcnt, RCUREF_SATURATED);
	return true;
}
EXPORT_SYMBOL_GPL(rcuref_get_slowpath);

 
bool rcuref_put_slowpath(rcuref_t *ref)
{
	unsigned int cnt = atomic_read(&ref->refcnt);

	 
	if (likely(cnt == RCUREF_NOREF)) {
		 
		if (atomic_cmpxchg_release(&ref->refcnt, RCUREF_NOREF, RCUREF_DEAD) != RCUREF_NOREF)
			return false;

		 
		smp_acquire__after_ctrl_dep();
		return true;
	}

	 
	if (WARN_ONCE(cnt >= RCUREF_RELEASED, "rcuref - imbalanced put()")) {
		atomic_set(&ref->refcnt, RCUREF_DEAD);
		return false;
	}

	 
	if (cnt > RCUREF_MAXREF)
		atomic_set(&ref->refcnt, RCUREF_SATURATED);
	return false;
}
EXPORT_SYMBOL_GPL(rcuref_put_slowpath);
