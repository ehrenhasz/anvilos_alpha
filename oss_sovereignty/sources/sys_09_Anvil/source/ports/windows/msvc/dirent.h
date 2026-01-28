
#ifndef MICROPY_INCLUDED_WINDOWS_MSVC_DIRENT_H
#define MICROPY_INCLUDED_WINDOWS_MSVC_DIRENT_H




#include <sys/types.h>

#define _DIRENT_HAVE_D_TYPE (1)
#define DTTOIF dttoif


typedef struct DIR DIR;




typedef struct dirent {
    ino_t d_ino;
    int d_type;
    char *d_name;
} dirent;

DIR *opendir(const char *name);
int closedir(DIR *dir);
struct dirent *readdir(DIR *dir);
int dttoif(int d_type);

#endif 
