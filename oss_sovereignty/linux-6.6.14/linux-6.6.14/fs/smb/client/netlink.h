#ifndef _CIFS_NETLINK_H
#define _CIFS_NETLINK_H
extern struct genl_family cifs_genl_family;
extern int cifs_genl_init(void);
extern void cifs_genl_exit(void);
#endif  
