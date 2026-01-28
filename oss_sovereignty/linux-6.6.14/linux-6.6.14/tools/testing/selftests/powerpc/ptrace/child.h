#include <stdio.h>
#include <stdbool.h>
#include <semaphore.h>
struct child_sync {
	sem_t sem_parent;
	bool parent_gave_up;
	sem_t sem_child;
	bool child_gave_up;
};
#define CHILD_FAIL_IF(x, sync)						\
	do {								\
		if (x) {						\
			fprintf(stderr,					\
				"[FAIL] Test FAILED on line %d\n", __LINE__); \
			(sync)->child_gave_up = true;			\
			prod_parent(sync);				\
			return 1;					\
		}							\
	} while (0)
#define PARENT_FAIL_IF(x, sync)						\
	do {								\
		if (x) {						\
			fprintf(stderr,					\
				"[FAIL] Test FAILED on line %d\n", __LINE__); \
			(sync)->parent_gave_up = true;			\
			prod_child(sync);				\
			return 1;					\
		}							\
	} while (0)
#define PARENT_SKIP_IF_UNSUPPORTED(x, sync, msg)			\
	do {								\
		if ((x) == -1 && (errno == ENODEV || errno == EINVAL)) { \
			(sync)->parent_gave_up = true;			\
			prod_child(sync);				\
			SKIP_IF_MSG(1, msg);				\
		}							\
	} while (0)
int init_child_sync(struct child_sync *sync)
{
	int ret;
	ret = sem_init(&sync->sem_parent, 1, 0);
	if (ret) {
		perror("Semaphore initialization failed");
		return 1;
	}
	ret = sem_init(&sync->sem_child, 1, 0);
	if (ret) {
		perror("Semaphore initialization failed");
		return 1;
	}
	return 0;
}
void destroy_child_sync(struct child_sync *sync)
{
	sem_destroy(&sync->sem_parent);
	sem_destroy(&sync->sem_child);
}
int wait_child(struct child_sync *sync)
{
	int ret;
	ret = sem_wait(&sync->sem_parent);
	if (ret) {
		perror("Error waiting for child");
		return 1;
	}
	return sync->child_gave_up;
}
int prod_child(struct child_sync *sync)
{
	int ret;
	ret = sem_post(&sync->sem_child);
	if (ret) {
		perror("Error prodding child");
		return 1;
	}
	return 0;
}
int wait_parent(struct child_sync *sync)
{
	int ret;
	ret = sem_wait(&sync->sem_child);
	if (ret) {
		perror("Error waiting for parent");
		return 1;
	}
	return sync->parent_gave_up;
}
int prod_parent(struct child_sync *sync)
{
	int ret;
	ret = sem_post(&sync->sem_parent);
	if (ret) {
		perror("Error prodding parent");
		return 1;
	}
	return 0;
}
