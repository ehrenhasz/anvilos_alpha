

#ifdef __QNXNTO__
# include <sys/procmgr.h>
#endif

#define	USE_TMPNAM

#define POSIX	    

#if defined(FEAT_GUI_PHOTON)
extern int is_photon_available;
#endif
