 

#ifndef _PORT_LINUX_H
#define _PORT_LINUX_H

#ifdef WITH_SELINUX
int ssh_selinux_enabled(void);
void ssh_selinux_setup_pty(char *, const char *);
void ssh_selinux_setup_exec_context(char *);
void ssh_selinux_change_context(const char *);
void ssh_selinux_setfscreatecon(const char *);
#endif

#ifdef LINUX_OOM_ADJUST
void oom_adjust_restore(void);
void oom_adjust_setup(void);
#endif

#endif  
