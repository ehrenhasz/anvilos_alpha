 

#ifndef UTIMECMP_H
#define UTIMECMP_H 1

#include <sys/types.h>
#include <sys/stat.h>

 
enum
{
   
  UTIMECMP_TRUNCATE_SOURCE = 1
};

int utimecmp (char const *, struct stat const *, struct stat const *, int);
int utimecmpat (int, char const *, struct stat const *, struct stat const *,
                int);

#endif
