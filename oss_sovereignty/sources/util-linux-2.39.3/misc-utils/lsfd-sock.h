
#ifndef UTIL_LINUX_LSFD_SOCK_H
#define UTIL_LINUX_LSFD_SOCK_H

#include <stdbool.h>
#include <sys/stat.h>

#include "libsmartcols.h"


struct sock_xinfo {
	ino_t inode;		
	ino_t netns_inode;	
	const struct sock_xinfo_class *class;
};

struct sock {
	struct file file;
	char *protoname;
	struct sock_xinfo *xinfo;
};

struct sock_xinfo_class {
	
	char * (*get_name)(struct sock_xinfo *, struct sock *);
	char * (*get_type)(struct sock_xinfo *, struct sock *);
	char * (*get_state)(struct sock_xinfo *, struct sock *);
	bool (*get_listening)(struct sock_xinfo *, struct sock *);
	
	bool (*fill_column)(struct proc *,
			    struct sock_xinfo *,
			    struct sock *,
			    struct libscols_line *,
			    int,
			    size_t,
			    char **str);

	void (*free)(struct sock_xinfo *);
};

void initialize_sock_xinfos(void);
void finalize_sock_xinfos(void);

struct sock_xinfo *get_sock_xinfo(ino_t netns_inode);

#endif 
