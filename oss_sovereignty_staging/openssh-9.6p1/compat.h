 

 

#ifndef COMPAT_H
#define COMPAT_H

#define SSH_BUG_UTF8TTYMODE	0x00000001
#define SSH_BUG_SIGTYPE		0x00000002
#define SSH_BUG_SIGTYPE74	0x00000004
 
#define SSH_OLD_SESSIONID	0x00000010
 
#define SSH_BUG_DEBUG		0x00000040
 
 
 
 
#define SSH_BUG_SCANNER		0x00000800
 
 
#define SSH_OLD_DHGEX		0x00004000
#define SSH_BUG_NOREKEY		0x00008000
 
 
 
 
#define SSH_BUG_EXTEOF		0x00200000
#define SSH_BUG_PROBE		0x00400000
 
#define SSH_OLD_FORWARD_ADDR	0x01000000
 
#define SSH_NEW_OPENSSH		0x04000000
#define SSH_BUG_DYNAMIC_RPORT	0x08000000
#define SSH_BUG_CURVE25519PAD	0x10000000
#define SSH_BUG_HOSTKEYS	0x20000000
#define SSH_BUG_DHGEX_LARGE	0x40000000

struct ssh;

void    compat_banner(struct ssh *, const char *);
char	*compat_kex_proposal(struct ssh *, const char *);
#endif
