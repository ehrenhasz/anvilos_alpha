

#ifndef SELFTESTS_SW_SYNC_H
#define SELFTESTS_SW_SYNC_H



int sw_sync_timeline_create(void);
int sw_sync_timeline_is_valid(int fd);
int sw_sync_timeline_inc(int fd, unsigned int count);
void sw_sync_timeline_destroy(int fd);

int sw_sync_fence_create(int fd, const char *name, unsigned int value);
int sw_sync_fence_is_valid(int fd);
void sw_sync_fence_destroy(int fd);

#endif
