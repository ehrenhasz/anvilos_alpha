 

 

#ifndef INCLUDES_H
#define INCLUDES_H

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  
#endif

#include <sys/types.h>
#include <sys/socket.h>  

#ifdef HAVE_LIMITS_H
# include <limits.h>  
#endif
#ifdef HAVE_BSTRING_H
# include <bstring.h>
#endif
#ifdef HAVE_ENDIAN_H
# include <endian.h>
#endif
#ifdef HAVE_TTYENT_H
# include <ttyent.h>
#endif
#ifdef HAVE_UTIME_H
# include <utime.h>
#endif
#ifdef HAVE_MAILLOCK_H
# include <maillock.h>  
#endif
#ifdef HAVE_NEXT
# include <libc.h>
#endif
#ifdef HAVE_PATHS_H
# include <paths.h>
#endif

 
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_LOGIN_H
# include <login.h>
#endif

#ifdef HAVE_UTMP_H
#  include <utmp.h>
#endif
#ifdef HAVE_UTMPX_H
#  include <utmpx.h>
#endif
#ifdef HAVE_LASTLOG_H
#  include <lastlog.h>
#endif

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifdef HAVE_SYS_BSDTTY_H
# include <sys/bsdtty.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <termios.h>
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>  
#endif
#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>  
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>  
#endif
#ifdef HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>  
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>  
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>  
#endif
#ifdef HAVE_SYS_STRTIO_H
#include <sys/strtio.h>	 
#endif
#if defined(HAVE_SYS_PTMS_H) && defined(HAVE_DEV_PTMX)
# if defined(HAVE_SYS_STREAM_H)
#  include <sys/stream.h>	 
# endif
#include <sys/ptms.h>	 
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>  
#ifdef HAVE_RPC_TYPES_H
# include <rpc/types.h>  
#endif
#ifdef USE_PAM
#if defined(HAVE_SECURITY_PAM_APPL_H)
# include <security/pam_appl.h>
#elif defined (HAVE_PAM_PAM_APPL_H)
# include <pam/pam_appl.h>
#endif
#endif
#ifdef HAVE_READPASSPHRASE_H
# include <readpassphrase.h>
#endif

#ifdef HAVE_IA_H
# include <ia.h>
#endif

#ifdef HAVE_IAF_H
# include <iaf.h>
#endif

#ifdef HAVE_TMPDIR_H
# include <tmpdir.h>
#endif

#if defined(HAVE_BSD_LIBUTIL_H)
# include <bsd/libutil.h>
#elif defined(HAVE_LIBUTIL_H)
# include <libutil.h>
#endif

#if defined(KRB5) && defined(USE_AFS)
# include <krb5.h>
# include <kafs.h>
#endif

#if defined(HAVE_SYS_SYSLOG_H)
# include <sys/syslog.h>
#endif

#include <errno.h>

 
#ifdef GETSPNAM_CONFLICTING_DEFS
# ifdef _INCLUDE__STDC__
#  undef _INCLUDE__STDC__
# endif
#endif

#ifdef WITH_OPENSSL
#include <openssl/opensslv.h>  
#endif

#include "defines.h"

#include "platform.h"
#include "openbsd-compat/openbsd-compat.h"
#include "openbsd-compat/bsd-nextstep.h"

#include "entropy.h"

#endif  
