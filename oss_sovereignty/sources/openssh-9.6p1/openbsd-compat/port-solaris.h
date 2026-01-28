

#ifndef _PORT_SOLARIS_H

#include <sys/types.h>

#include <pwd.h>

void solaris_contract_pre_fork(void);
void solaris_contract_post_fork_child(void);
void solaris_contract_post_fork_parent(pid_t pid);
void solaris_set_default_project(struct passwd *);
# ifdef USE_SOLARIS_PRIVS
#include <priv.h>
priv_set_t *solaris_basic_privset(void);
void solaris_drop_privs_pinfo_net_fork_exec(void);
void solaris_drop_privs_root_pinfo_net(void);
void solaris_drop_privs_root_pinfo_net_exec(void);
# endif 

#endif
