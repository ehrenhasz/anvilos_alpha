
#ifndef MICROPY_INCLUDED_PY_MPERRNO_H
#define MICROPY_INCLUDED_PY_MPERRNO_H

#include "py/mpconfig.h"

#if MICROPY_USE_INTERNAL_ERRNO




#define MP_EPERM              (1) 
#define MP_ENOENT             (2) 
#define MP_ESRCH              (3) 
#define MP_EINTR              (4) 
#define MP_EIO                (5) 
#define MP_ENXIO              (6) 
#define MP_E2BIG              (7) 
#define MP_ENOEXEC            (8) 
#define MP_EBADF              (9) 
#define MP_ECHILD            (10) 
#define MP_EAGAIN            (11) 
#define MP_ENOMEM            (12) 
#define MP_EACCES            (13) 
#define MP_EFAULT            (14) 
#define MP_ENOTBLK           (15) 
#define MP_EBUSY             (16) 
#define MP_EEXIST            (17) 
#define MP_EXDEV             (18) 
#define MP_ENODEV            (19) 
#define MP_ENOTDIR           (20) 
#define MP_EISDIR            (21) 
#define MP_EINVAL            (22) 
#define MP_ENFILE            (23) 
#define MP_EMFILE            (24) 
#define MP_ENOTTY            (25) 
#define MP_ETXTBSY           (26) 
#define MP_EFBIG             (27) 
#define MP_ENOSPC            (28) 
#define MP_ESPIPE            (29) 
#define MP_EROFS             (30) 
#define MP_EMLINK            (31) 
#define MP_EPIPE             (32) 
#define MP_EDOM              (33) 
#define MP_ERANGE            (34) 
#define MP_EWOULDBLOCK  MP_EAGAIN 
#define MP_EOPNOTSUPP        (95) 
#define MP_EAFNOSUPPORT      (97) 
#define MP_EADDRINUSE        (98) 
#define MP_ECONNABORTED     (103) 
#define MP_ECONNRESET       (104) 
#define MP_ENOBUFS          (105) 
#define MP_EISCONN          (106) 
#define MP_ENOTCONN         (107) 
#define MP_ETIMEDOUT        (110) 
#define MP_ECONNREFUSED     (111) 
#define MP_EHOSTUNREACH     (113) 
#define MP_EALREADY         (114) 
#define MP_EINPROGRESS      (115) 
#define MP_ECANCELED        (125) 

#else



#include <errno.h>

#define MP_EPERM            EPERM
#define MP_ENOENT           ENOENT
#define MP_ESRCH            ESRCH
#define MP_EINTR            EINTR
#define MP_EIO              EIO
#define MP_ENXIO            ENXIO
#define MP_E2BIG            E2BIG
#define MP_ENOEXEC          ENOEXEC
#define MP_EBADF            EBADF
#define MP_ECHILD           ECHILD
#define MP_EAGAIN           EAGAIN
#define MP_ENOMEM           ENOMEM
#define MP_EACCES           EACCES
#define MP_EFAULT           EFAULT
#define MP_ENOTBLK          ENOTBLK
#define MP_EBUSY            EBUSY
#define MP_EEXIST           EEXIST
#define MP_EXDEV            EXDEV
#define MP_ENODEV           ENODEV
#define MP_ENOTDIR          ENOTDIR
#define MP_EISDIR           EISDIR
#define MP_EINVAL           EINVAL
#define MP_ENFILE           ENFILE
#define MP_EMFILE           EMFILE
#define MP_ENOTTY           ENOTTY
#define MP_ETXTBSY          ETXTBSY
#define MP_EFBIG            EFBIG
#define MP_ENOSPC           ENOSPC
#define MP_ESPIPE           ESPIPE
#define MP_EROFS            EROFS
#define MP_EMLINK           EMLINK
#define MP_EPIPE            EPIPE
#define MP_EDOM             EDOM
#define MP_ERANGE           ERANGE
#define MP_EWOULDBLOCK      EWOULDBLOCK
#define MP_EOPNOTSUPP       EOPNOTSUPP
#define MP_EAFNOSUPPORT     EAFNOSUPPORT
#define MP_EADDRINUSE       EADDRINUSE
#define MP_ECONNABORTED     ECONNABORTED
#define MP_ECONNRESET       ECONNRESET
#define MP_ENOBUFS          ENOBUFS
#define MP_EISCONN          EISCONN
#define MP_ENOTCONN         ENOTCONN
#define MP_ETIMEDOUT        ETIMEDOUT
#define MP_ECONNREFUSED     ECONNREFUSED
#define MP_EHOSTUNREACH     EHOSTUNREACH
#define MP_EALREADY         EALREADY
#define MP_EINPROGRESS      EINPROGRESS
#define MP_ECANCELED        ECANCELED

#endif

#if MICROPY_PY_ERRNO

#include "py/obj.h"

qstr mp_errno_to_str(mp_obj_t errno_val);

#endif

#endif 
