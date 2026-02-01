
 

#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/refcount.h>

#include <media/media-device.h>
#include <media/media-request.h>

static const char * const request_state[] = {
	[MEDIA_REQUEST_STATE_IDLE]	 = "idle",
	[MEDIA_REQUEST_STATE_VALIDATING] = "validating",
	[MEDIA_REQUEST_STATE_QUEUED]	 = "queued",
	[MEDIA_REQUEST_STATE_COMPLETE]	 = "complete",
	[MEDIA_REQUEST_STATE_CLEANING]	 = "cleaning",
	[MEDIA_REQUEST_STATE_UPDATING]	 = "updating",
};

static const char *
media_request_state_str(enum media_request_state state)
{
	BUILD_BUG_ON(ARRAY_SIZE(request_state) != NR_OF_MEDIA_REQUEST_STATE);

	if (WARN_ON(state >= ARRAY_SIZE(request_state)))
		return "invalid";
	return request_state[state];
}

static void media_request_clean(struct media_request *req)
{
	struct media_request_object *obj, *obj_safe;

	 
	WARN_ON(req->state != MEDIA_REQUEST_STATE_CLEANING);
	WARN_ON(req->updating_count);
	WARN_ON(req->access_count);

	list_for_each_entry_safe(obj, obj_safe, &req->objects, list) {
		media_request_object_unbind(obj);
		media_request_object_put(obj);
	}

	req->updating_count = 0;
	req->access_count = 0;
	WARN_ON(req->num_incomplete_objects);
	req->num_incomplete_objects = 0;
	wake_up_interruptible_all(&req->poll_wait);
}

static void media_request_release(struct kref *kref)
{
	struct media_request *req =
		container_of(kref, struct media_request, kref);
	struct media_device *mdev = req->mdev;

	dev_dbg(mdev->dev, "request: release %s\n", req->debug_str);

	 
	req->state = MEDIA_REQUEST_STATE_CLEANING;

	media_request_clean(req);

	if (mdev->ops->req_free)
		mdev->ops->req_free(req);
	else
		kfree(req);
}

void media_request_put(struct media_request *req)
{
	kref_put(&req->kref, media_request_release);
}
EXPORT_SYMBOL_GPL(media_request_put);

static int media_request_close(struct inode *inode, struct file *filp)
{
	struct media_request *req = filp->private_data;

	media_request_put(req);
	return 0;
}

static __poll_t media_request_poll(struct file *filp,
				   struct poll_table_struct *wait)
{
	struct media_request *req = filp->private_data;
	unsigned long flags;
	__poll_t ret = 0;

	if (!(poll_requested_events(wait) & EPOLLPRI))
		return 0;

	poll_wait(filp, &req->poll_wait, wait);
	spin_lock_irqsave(&req->lock, flags);
	if (req->state == MEDIA_REQUEST_STATE_COMPLETE) {
		ret = EPOLLPRI;
		goto unlock;
	}
	if (req->state != MEDIA_REQUEST_STATE_QUEUED) {
		ret = EPOLLERR;
		goto unlock;
	}

unlock:
	spin_unlock_irqrestore(&req->lock, flags);
	return ret;
}

static long media_request_ioctl_queue(struct media_request *req)
{
	struct media_device *mdev = req->mdev;
	enum media_request_state state;
	unsigned long flags;
	int ret;

	dev_dbg(mdev->dev, "request: queue %s\n", req->debug_str);

	 
	mutex_lock(&mdev->req_queue_mutex);

	media_request_get(req);

	spin_lock_irqsave(&req->lock, flags);
	if (req->state == MEDIA_REQUEST_STATE_IDLE)
		req->state = MEDIA_REQUEST_STATE_VALIDATING;
	state = req->state;
	spin_unlock_irqrestore(&req->lock, flags);
	if (state != MEDIA_REQUEST_STATE_VALIDATING) {
		dev_dbg(mdev->dev,
			"request: unable to queue %s, request in state %s\n",
			req->debug_str, media_request_state_str(state));
		media_request_put(req);
		mutex_unlock(&mdev->req_queue_mutex);
		return -EBUSY;
	}

	ret = mdev->ops->req_validate(req);

	 
	spin_lock_irqsave(&req->lock, flags);
	req->state = ret ? MEDIA_REQUEST_STATE_IDLE
			 : MEDIA_REQUEST_STATE_QUEUED;
	spin_unlock_irqrestore(&req->lock, flags);

	if (!ret)
		mdev->ops->req_queue(req);

	mutex_unlock(&mdev->req_queue_mutex);

	if (ret) {
		dev_dbg(mdev->dev, "request: can't queue %s (%d)\n",
			req->debug_str, ret);
		media_request_put(req);
	}

	return ret;
}

static long media_request_ioctl_reinit(struct media_request *req)
{
	struct media_device *mdev = req->mdev;
	unsigned long flags;

	spin_lock_irqsave(&req->lock, flags);
	if (req->state != MEDIA_REQUEST_STATE_IDLE &&
	    req->state != MEDIA_REQUEST_STATE_COMPLETE) {
		dev_dbg(mdev->dev,
			"request: %s not in idle or complete state, cannot reinit\n",
			req->debug_str);
		spin_unlock_irqrestore(&req->lock, flags);
		return -EBUSY;
	}
	if (req->access_count) {
		dev_dbg(mdev->dev,
			"request: %s is being accessed, cannot reinit\n",
			req->debug_str);
		spin_unlock_irqrestore(&req->lock, flags);
		return -EBUSY;
	}
	req->state = MEDIA_REQUEST_STATE_CLEANING;
	spin_unlock_irqrestore(&req->lock, flags);

	media_request_clean(req);

	spin_lock_irqsave(&req->lock, flags);
	req->state = MEDIA_REQUEST_STATE_IDLE;
	spin_unlock_irqrestore(&req->lock, flags);

	return 0;
}

static long media_request_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct media_request *req = filp->private_data;

	switch (cmd) {
	case MEDIA_REQUEST_IOC_QUEUE:
		return media_request_ioctl_queue(req);
	case MEDIA_REQUEST_IOC_REINIT:
		return media_request_ioctl_reinit(req);
	default:
		return -ENOIOCTLCMD;
	}
}

static const struct file_operations request_fops = {
	.owner = THIS_MODULE,
	.poll = media_request_poll,
	.unlocked_ioctl = media_request_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = media_request_ioctl,
#endif  
	.release = media_request_close,
};

struct media_request *
media_request_get_by_fd(struct media_device *mdev, int request_fd)
{
	struct fd f;
	struct media_request *req;

	if (!mdev || !mdev->ops ||
	    !mdev->ops->req_validate || !mdev->ops->req_queue)
		return ERR_PTR(-EBADR);

	f = fdget(request_fd);
	if (!f.file)
		goto err_no_req_fd;

	if (f.file->f_op != &request_fops)
		goto err_fput;
	req = f.file->private_data;
	if (req->mdev != mdev)
		goto err_fput;

	 
	media_request_get(req);
	fdput(f);

	return req;

err_fput:
	fdput(f);

err_no_req_fd:
	dev_dbg(mdev->dev, "cannot find request_fd %d\n", request_fd);
	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL_GPL(media_request_get_by_fd);

int media_request_alloc(struct media_device *mdev, int *alloc_fd)
{
	struct media_request *req;
	struct file *filp;
	int fd;
	int ret;

	 
	if (WARN_ON(!mdev->ops->req_alloc ^ !mdev->ops->req_free))
		return -ENOMEM;

	if (mdev->ops->req_alloc)
		req = mdev->ops->req_alloc(mdev);
	else
		req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		goto err_free_req;
	}

	filp = anon_inode_getfile("request", &request_fops, NULL, O_CLOEXEC);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto err_put_fd;
	}

	filp->private_data = req;
	req->mdev = mdev;
	req->state = MEDIA_REQUEST_STATE_IDLE;
	req->num_incomplete_objects = 0;
	kref_init(&req->kref);
	INIT_LIST_HEAD(&req->objects);
	spin_lock_init(&req->lock);
	init_waitqueue_head(&req->poll_wait);
	req->updating_count = 0;
	req->access_count = 0;

	*alloc_fd = fd;

	snprintf(req->debug_str, sizeof(req->debug_str), "%u:%d",
		 atomic_inc_return(&mdev->request_id), fd);
	dev_dbg(mdev->dev, "request: allocated %s\n", req->debug_str);

	fd_install(fd, filp);

	return 0;

err_put_fd:
	put_unused_fd(fd);

err_free_req:
	if (mdev->ops->req_free)
		mdev->ops->req_free(req);
	else
		kfree(req);

	return ret;
}

static void media_request_object_release(struct kref *kref)
{
	struct media_request_object *obj =
		container_of(kref, struct media_request_object, kref);
	struct media_request *req = obj->req;

	if (WARN_ON(req))
		media_request_object_unbind(obj);
	obj->ops->release(obj);
}

struct media_request_object *
media_request_object_find(struct media_request *req,
			  const struct media_request_object_ops *ops,
			  void *priv)
{
	struct media_request_object *obj;
	struct media_request_object *found = NULL;
	unsigned long flags;

	if (WARN_ON(!ops || !priv))
		return NULL;

	spin_lock_irqsave(&req->lock, flags);
	list_for_each_entry(obj, &req->objects, list) {
		if (obj->ops == ops && obj->priv == priv) {
			media_request_object_get(obj);
			found = obj;
			break;
		}
	}
	spin_unlock_irqrestore(&req->lock, flags);
	return found;
}
EXPORT_SYMBOL_GPL(media_request_object_find);

void media_request_object_put(struct media_request_object *obj)
{
	kref_put(&obj->kref, media_request_object_release);
}
EXPORT_SYMBOL_GPL(media_request_object_put);

void media_request_object_init(struct media_request_object *obj)
{
	obj->ops = NULL;
	obj->req = NULL;
	obj->priv = NULL;
	obj->completed = false;
	INIT_LIST_HEAD(&obj->list);
	kref_init(&obj->kref);
}
EXPORT_SYMBOL_GPL(media_request_object_init);

int media_request_object_bind(struct media_request *req,
			      const struct media_request_object_ops *ops,
			      void *priv, bool is_buffer,
			      struct media_request_object *obj)
{
	unsigned long flags;
	int ret = -EBUSY;

	if (WARN_ON(!ops->release))
		return -EBADR;

	spin_lock_irqsave(&req->lock, flags);

	if (WARN_ON(req->state != MEDIA_REQUEST_STATE_UPDATING &&
		    req->state != MEDIA_REQUEST_STATE_QUEUED))
		goto unlock;

	obj->req = req;
	obj->ops = ops;
	obj->priv = priv;

	if (is_buffer)
		list_add_tail(&obj->list, &req->objects);
	else
		list_add(&obj->list, &req->objects);
	req->num_incomplete_objects++;
	ret = 0;

unlock:
	spin_unlock_irqrestore(&req->lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(media_request_object_bind);

void media_request_object_unbind(struct media_request_object *obj)
{
	struct media_request *req = obj->req;
	unsigned long flags;
	bool completed = false;

	if (WARN_ON(!req))
		return;

	spin_lock_irqsave(&req->lock, flags);
	list_del(&obj->list);
	obj->req = NULL;

	if (req->state == MEDIA_REQUEST_STATE_COMPLETE)
		goto unlock;

	if (WARN_ON(req->state == MEDIA_REQUEST_STATE_VALIDATING))
		goto unlock;

	if (req->state == MEDIA_REQUEST_STATE_CLEANING) {
		if (!obj->completed)
			req->num_incomplete_objects--;
		goto unlock;
	}

	if (WARN_ON(!req->num_incomplete_objects))
		goto unlock;

	req->num_incomplete_objects--;
	if (req->state == MEDIA_REQUEST_STATE_QUEUED &&
	    !req->num_incomplete_objects) {
		req->state = MEDIA_REQUEST_STATE_COMPLETE;
		completed = true;
		wake_up_interruptible_all(&req->poll_wait);
	}

unlock:
	spin_unlock_irqrestore(&req->lock, flags);
	if (obj->ops->unbind)
		obj->ops->unbind(obj);
	if (completed)
		media_request_put(req);
}
EXPORT_SYMBOL_GPL(media_request_object_unbind);

void media_request_object_complete(struct media_request_object *obj)
{
	struct media_request *req = obj->req;
	unsigned long flags;
	bool completed = false;

	spin_lock_irqsave(&req->lock, flags);
	if (obj->completed)
		goto unlock;
	obj->completed = true;
	if (WARN_ON(!req->num_incomplete_objects) ||
	    WARN_ON(req->state != MEDIA_REQUEST_STATE_QUEUED))
		goto unlock;

	if (!--req->num_incomplete_objects) {
		req->state = MEDIA_REQUEST_STATE_COMPLETE;
		wake_up_interruptible_all(&req->poll_wait);
		completed = true;
	}
unlock:
	spin_unlock_irqrestore(&req->lock, flags);
	if (completed)
		media_request_put(req);
}
EXPORT_SYMBOL_GPL(media_request_object_complete);
