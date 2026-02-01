 

#ifndef _PORT_TUN_H
#define _PORT_TUN_H

struct Channel;
struct ssh;

#if defined(SSH_TUN_LINUX) || defined(SSH_TUN_FREEBSD)
# define CUSTOM_SYS_TUN_OPEN
int	  sys_tun_open(int, int, char **);
#endif

#if defined(SSH_TUN_COMPAT_AF) || defined(SSH_TUN_PREPEND_AF)
# define SSH_TUN_FILTER
int	 sys_tun_infilter(struct ssh *, struct Channel *, char *, int);
u_char	*sys_tun_outfilter(struct ssh *, struct Channel *, u_char **, size_t *);
#endif

#if defined(SYS_RDOMAIN_LINUX)
# define HAVE_SYS_GET_RDOMAIN
# define HAVE_SYS_SET_RDOMAIN
# define HAVE_SYS_VALID_RDOMAIN
char *sys_get_rdomain(int fd);
int sys_set_rdomain(int fd, const char *name);
int sys_valid_rdomain(const char *name);
#endif

#if defined(SYS_RDOMAIN_XXX)
# define HAVE_SYS_SET_PROCESS_RDOMAIN
void sys_set_process_rdomain(const char *name);
#endif

#endif
