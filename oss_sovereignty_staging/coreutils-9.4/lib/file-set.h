 

#include <sys/types.h>
#include <sys/stat.h>

#include "hash.h"

extern void record_file (Hash_table *ht, char const *file,
                         struct stat const *stats)
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || defined __clang__
  __attribute__ ((__nonnull__ (2, 3)))
#endif
;

extern bool seen_file (Hash_table const *ht, char const *file,
                       struct stat const *stats);
