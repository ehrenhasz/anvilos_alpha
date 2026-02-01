 

#ifndef	ZED_FILE_H
#define	ZED_FILE_H

#include <sys/types.h>
#include <unistd.h>

int zed_file_lock(int fd);

int zed_file_unlock(int fd);

pid_t zed_file_is_locked(int fd);

void zed_file_close_from(int fd);

#endif	 
