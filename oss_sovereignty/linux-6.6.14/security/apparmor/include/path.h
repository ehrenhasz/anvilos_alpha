 
 

#ifndef __AA_PATH_H
#define __AA_PATH_H

enum path_flags {
	PATH_IS_DIR = 0x1,		 
	PATH_CONNECT_PATH = 0x4,	 
	PATH_CHROOT_REL = 0x8,		 
	PATH_CHROOT_NSCONNECT = 0x10,	 

	PATH_DELEGATE_DELETED = 0x10000,  
	PATH_MEDIATE_DELETED = 0x20000,	  
};

int aa_path_name(const struct path *path, int flags, char *buffer,
		 const char **name, const char **info,
		 const char *disconnected);

#define IN_ATOMIC true
char *aa_get_buffer(bool in_atomic);
void aa_put_buffer(char *buf);

#endif  
