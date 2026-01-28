

#include "py/mpconfig.h"



#define FFCONF_DEF  86604   



#define FF_FS_READONLY  0



#define FF_FS_MINIMIZE  0



#define FF_USE_STRFUNC  0



#define FF_USE_FIND     0



#define FF_USE_MKFS     1



#define FF_USE_FASTSEEK 0



#define FF_USE_EXPAND   0



#define FF_USE_CHMOD    1



#ifdef MICROPY_FATFS_USE_LABEL
#define FF_USE_LABEL    (MICROPY_FATFS_USE_LABEL)
#else
#define FF_USE_LABEL    0
#endif



#define FF_USE_FORWARD  0





#ifdef MICROPY_FATFS_LFN_CODE_PAGE
#define FF_CODE_PAGE MICROPY_FATFS_LFN_CODE_PAGE
#else
#define FF_CODE_PAGE 437
#endif



#ifdef MICROPY_FATFS_ENABLE_LFN
#define FF_USE_LFN  (MICROPY_FATFS_ENABLE_LFN)
#else
#define FF_USE_LFN  0
#endif
#ifdef MICROPY_FATFS_MAX_LFN
#define FF_MAX_LFN  (MICROPY_FATFS_MAX_LFN)
#else
#define FF_MAX_LFN  255
#endif



#define FF_LFN_UNICODE  0



#define FF_LFN_BUF      255
#define FF_SFN_BUF      12



#define FF_STRF_ENCODE  3



#ifdef MICROPY_FATFS_RPATH
#define FF_FS_RPATH (MICROPY_FATFS_RPATH)
#else
#define FF_FS_RPATH 0
#endif





#define FF_VOLUMES      1



#define FF_STR_VOLUME_ID    0
#define FF_VOLUME_STRS      "RAM","NAND","CF","SD","SD2","USB","USB2","USB3"



#ifdef MICROPY_FATFS_MULTI_PARTITION
#define FF_MULTI_PARTITION  (MICROPY_FATFS_MULTI_PARTITION)
#else
#define FF_MULTI_PARTITION  0
#endif



#define FF_MIN_SS   512
#ifdef MICROPY_FATFS_MAX_SS
#define FF_MAX_SS   (MICROPY_FATFS_MAX_SS)
#else
#define FF_MAX_SS   512
#endif



#define FF_USE_TRIM     0



#define FF_FS_NOFSINFO  0






#define FF_FS_TINY      1



#ifdef MICROPY_FATFS_EXFAT
#define FF_FS_EXFAT (MICROPY_FATFS_EXFAT)
#else
#define FF_FS_EXFAT 0
#endif



#ifdef MICROPY_FATFS_NORTC
#define FF_FS_NORTC (MICROPY_FATFS_NORTC)
#else
#define FF_FS_NORTC 0
#endif
#define FF_NORTC_MON    1
#define FF_NORTC_MDAY   1
#define FF_NORTC_YEAR   2018



#define FF_FS_LOCK      0



#ifdef MICROPY_FATFS_REENTRANT
#define FF_FS_REENTRANT (MICROPY_FATFS_REENTRANT)
#else
#define FF_FS_REENTRANT 0
#endif


#ifdef MICROPY_FATFS_TIMEOUT
#define FF_FS_TIMEOUT   (MICROPY_FATFS_TIMEOUT)
#else
#define FF_FS_TIMEOUT   1000
#endif

#ifdef MICROPY_FATFS_SYNC_T
#define FF_SYNC_t       MICROPY_FATFS_SYNC_T
#else
#define FF_SYNC_t       HANDLE
#endif





