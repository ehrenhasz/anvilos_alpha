#ifndef TEE_PRIVATE_H
#define TEE_PRIVATE_H
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/types.h>
#define TEE_DEVICE_FLAG_REGISTERED	0x1
#define TEE_MAX_DEV_NAME_LEN		32
struct tee_device {
	char name[TEE_MAX_DEV_NAME_LEN];
	const struct tee_desc *desc;
	int id;
	unsigned int flags;
	struct device dev;
	struct cdev cdev;
	size_t num_users;
	struct completion c_no_users;
	struct mutex mutex;	 
	struct idr idr;
	struct tee_shm_pool *pool;
};
int tee_shm_get_fd(struct tee_shm *shm);
bool tee_device_get(struct tee_device *teedev);
void tee_device_put(struct tee_device *teedev);
void teedev_ctx_get(struct tee_context *ctx);
void teedev_ctx_put(struct tee_context *ctx);
struct tee_shm *tee_shm_alloc_user_buf(struct tee_context *ctx, size_t size);
struct tee_shm *tee_shm_register_user_buf(struct tee_context *ctx,
					  unsigned long addr, size_t length);
#endif  
