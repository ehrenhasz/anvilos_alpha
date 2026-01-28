

#ifndef SELFTESTS_SYNC_H
#define SELFTESTS_SYNC_H

#define FENCE_STATUS_ERROR	(-1)
#define FENCE_STATUS_ACTIVE	(0)
#define FENCE_STATUS_SIGNALED	(1)

int sync_wait(int fd, int timeout);
int sync_merge(const char *name, int fd1, int fd2);
int sync_fence_size(int fd);
int sync_fence_count_with_status(int fd, int status);

#endif
