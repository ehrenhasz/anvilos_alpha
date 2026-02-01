 

#ifndef USERSPEC_H
# define USERSPEC_H 1

# include <sys/types.h>

char const *
parse_user_spec (char const *spec_arg, uid_t *uid, gid_t *gid,
                 char **username_arg, char **groupname_arg);
char const *
parse_user_spec_warn (char const *spec_arg, uid_t *uid, gid_t *gid,
                      char **username_arg, char **groupname_arg, bool *pwarn);

#endif
