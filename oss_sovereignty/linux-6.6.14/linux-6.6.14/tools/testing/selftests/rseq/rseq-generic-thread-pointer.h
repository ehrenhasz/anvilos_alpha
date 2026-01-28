#ifndef _RSEQ_GENERIC_THREAD_POINTER
#define _RSEQ_GENERIC_THREAD_POINTER
#ifdef __cplusplus
extern "C" {
#endif
static inline void *rseq_thread_pointer(void)
{
	return __builtin_thread_pointer();
}
#ifdef __cplusplus
}
#endif
#endif
