 

#ifndef _FILENAME_H
#define _FILENAME_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


 
#if defined _WIN32 || defined __CYGWIN__ \
    || defined __EMX__ || defined __MSDOS__ || defined __DJGPP__
   
# define ISSLASH(C) ((C) == '/' || (C) == '\\')
   
# define _IS_DRIVE_LETTER(C) \
    (((C) >= 'A' && (C) <= 'Z') || ((C) >= 'a' && (C) <= 'z'))
   
# undef _IS_DRIVE_LETTER
# define _IS_DRIVE_LETTER(C) \
    (((unsigned int) (C) | ('a' - 'A')) - 'a' <= 'z' - 'a')
# define HAS_DEVICE(Filename) \
    (_IS_DRIVE_LETTER ((Filename)[0]) && (Filename)[1] == ':')
# define FILE_SYSTEM_PREFIX_LEN(Filename) (HAS_DEVICE (Filename) ? 2 : 0)
# ifdef __CYGWIN__
#  define FILE_SYSTEM_DRIVE_PREFIX_CAN_BE_RELATIVE 0
# else
    
#  define FILE_SYSTEM_DRIVE_PREFIX_CAN_BE_RELATIVE 1
# endif
# if FILE_SYSTEM_DRIVE_PREFIX_CAN_BE_RELATIVE
#  define IS_ABSOLUTE_FILE_NAME(Filename) \
     ISSLASH ((Filename)[FILE_SYSTEM_PREFIX_LEN (Filename)])
# else
#  define IS_ABSOLUTE_FILE_NAME(Filename) \
     (ISSLASH ((Filename)[0]) || HAS_DEVICE (Filename))
# endif
# define IS_RELATIVE_FILE_NAME(Filename) \
    (! (ISSLASH ((Filename)[0]) || HAS_DEVICE (Filename)))
# define IS_FILE_NAME_WITH_DIR(Filename) \
    (strchr ((Filename), '/') != NULL || strchr ((Filename), '\\') != NULL \
     || HAS_DEVICE (Filename))
#else
   
# define ISSLASH(C) ((C) == '/')
# define HAS_DEVICE(Filename) ((void) (Filename), 0)
# define FILE_SYSTEM_PREFIX_LEN(Filename) ((void) (Filename), 0)
# define FILE_SYSTEM_DRIVE_PREFIX_CAN_BE_RELATIVE 0
# define IS_ABSOLUTE_FILE_NAME(Filename) ISSLASH ((Filename)[0])
# define IS_RELATIVE_FILE_NAME(Filename) (! ISSLASH ((Filename)[0]))
# define IS_FILE_NAME_WITH_DIR(Filename) (strchr ((Filename), '/') != NULL)
#endif

 
#define IS_ABSOLUTE_PATH IS_ABSOLUTE_FILE_NAME
#define IS_PATH_WITH_DIR IS_FILE_NAME_WITH_DIR


#ifdef __cplusplus
}
#endif

#endif  
