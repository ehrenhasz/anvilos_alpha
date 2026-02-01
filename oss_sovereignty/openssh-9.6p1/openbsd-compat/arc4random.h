 

 

 

#include <sys/mman.h>

#include <signal.h>

 
#define _ARC4_LOCK()
#define _ARC4_UNLOCK()
#define _ARC4_ATFORK(f)

static inline void
_getentropy_fail(void)
{
	fatal("getentropy failed");
}

static volatile sig_atomic_t _rs_forked;

static inline void
_rs_forkhandler(void)
{
	_rs_forked = 1;
}

static inline void
_rs_forkdetect(void)
{
	static pid_t _rs_pid = 0;
	pid_t pid = getpid();

	if (_rs_pid == 0 || _rs_pid == 1 || _rs_pid != pid || _rs_forked) {
		_rs_pid = pid;
		_rs_forked = 0;
		if (rs)
			memset(rs, 0, sizeof(*rs));
	}
}

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp)
{
#if defined(MAP_ANON) && defined(MAP_PRIVATE)
	if ((*rsp = mmap(NULL, sizeof(**rsp), PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		return (-1);

	if ((*rsxp = mmap(NULL, sizeof(**rsxp), PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED) {
		munmap(*rsp, sizeof(**rsp));
		*rsp = NULL;
		return (-1);
	}
#else
	if ((*rsp = calloc(1, sizeof(**rsp))) == NULL)
		return (-1);
	if ((*rsxp = calloc(1, sizeof(**rsxp))) == NULL) {
		free(*rsp);
		*rsp = NULL;
		return (-1);
	}
#endif

	_ARC4_ATFORK(_rs_forkhandler);
	return (0);
}
