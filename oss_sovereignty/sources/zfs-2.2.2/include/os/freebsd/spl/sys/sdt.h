

#ifndef _OPENSOLARIS_SYS_SDT_H_
#define	_OPENSOLARIS_SYS_SDT_H_

#include_next <sys/sdt.h>
#ifdef KDTRACE_HOOKS

SDT_PROBE_DECLARE(sdt, , , set__error);

#define	SET_ERROR(err) \
	((sdt_sdt___set__error->id ? \
	(*sdt_probe_func)(sdt_sdt___set__error->id, \
	    (uintptr_t)err, 0, 0, 0, 0) : 0), err)
#else
#define	SET_ERROR(err) (err)
#endif

#endif	
