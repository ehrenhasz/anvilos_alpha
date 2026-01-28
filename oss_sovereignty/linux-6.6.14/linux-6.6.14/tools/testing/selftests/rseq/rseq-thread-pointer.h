#ifndef _RSEQ_THREAD_POINTER
#define _RSEQ_THREAD_POINTER
#if defined(__x86_64__) || defined(__i386__)
#include "rseq-x86-thread-pointer.h"
#elif defined(__PPC__)
#include "rseq-ppc-thread-pointer.h"
#else
#include "rseq-generic-thread-pointer.h"
#endif
#endif
