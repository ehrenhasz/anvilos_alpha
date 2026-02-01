 

#if defined(_KERNEL)
#if defined(HAVE_DECLARE_EVENT_CLASS)

#else

DEFINE_DTRACE_PROBE(zfs__rrwfastpath__rdmiss);
DEFINE_DTRACE_PROBE(zfs__rrwfastpath__exitmiss);

#endif  
#endif  
