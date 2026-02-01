 

#include <sys/types.h>

struct savewd;
bool make_dir_parents (char *dir,
                       struct savewd *wd,
                       int (*make_ancestor) (char const *, char const *,
                                             void *),
                       void *options,
                       mode_t mode,
                       void (*announce) (char const *, void *),
                       mode_t mode_bits,
                       uid_t owner,
                       gid_t group,
                       bool preserve_existing);
