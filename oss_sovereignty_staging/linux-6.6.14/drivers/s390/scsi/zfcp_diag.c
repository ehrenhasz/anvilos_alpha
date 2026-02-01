
 

#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "zfcp_diag.h"
#include "zfcp_ext.h"
#include "zfcp_def.h"

static DECLARE_WAIT_QUEUE_HEAD(__zfcp_diag_publish_wait);

 
int zfcp_diag_adapter_setup(struct zfcp_adapter *const adapter)
{
	struct zfcp_diag_adapter *diag;
	struct zfcp_diag_header *hdr;

	diag = kzalloc(sizeof(*diag), GFP_KERNEL);
	if (diag == NULL)
		return -ENOMEM;

	diag->max_age = (5 * 1000);  

	 
	hdr = &diag->port_data.header;

	spin_lock_init(&hdr->access_lock);
	hdr->buffer = &diag->port_data.data;
	hdr->buffer_size = sizeof(diag->port_data.data);
	 
	hdr->timestamp = jiffies - msecs_to_jiffies(diag->max_age);

	 
	hdr = &diag->config_data.header;

	spin_lock_init(&hdr->access_lock);
	hdr->buffer = &diag->config_data.data;
	hdr->buffer_size = sizeof(diag->config_data.data);
	 
	hdr->timestamp = jiffies - msecs_to_jiffies(diag->max_age);

	adapter->diagnostics = diag;
	return 0;
}

 
void zfcp_diag_adapter_free(struct zfcp_adapter *const adapter)
{
	kfree(adapter->diagnostics);
	adapter->diagnostics = NULL;
}

 
void zfcp_diag_update_xdata(struct zfcp_diag_header *const hdr,
			    const void *const data, const bool incomplete)
{
	const unsigned long capture_timestamp = jiffies;
	unsigned long flags;

	spin_lock_irqsave(&hdr->access_lock, flags);

	 
	if (!time_after_eq(capture_timestamp, hdr->timestamp))
		goto out;

	hdr->timestamp = capture_timestamp;
	hdr->incomplete = incomplete;
	memcpy(hdr->buffer, data, hdr->buffer_size);
out:
	spin_unlock_irqrestore(&hdr->access_lock, flags);
}

 
int zfcp_diag_update_port_data_buffer(struct zfcp_adapter *const adapter)
{
	int rc;

	rc = zfcp_fsf_exchange_port_data_sync(adapter->qdio, NULL);
	if (rc == -EAGAIN)
		rc = 0;  

	 

	return rc;
}

 
int zfcp_diag_update_config_data_buffer(struct zfcp_adapter *const adapter)
{
	int rc;

	rc = zfcp_fsf_exchange_config_data_sync(adapter->qdio, NULL);
	if (rc == -EAGAIN)
		rc = 0;  

	 

	return rc;
}

static int __zfcp_diag_update_buffer(struct zfcp_adapter *const adapter,
				     struct zfcp_diag_header *const hdr,
				     zfcp_diag_update_buffer_func buffer_update,
				     unsigned long *const flags)
	__must_hold(hdr->access_lock)
{
	int rc;

	if (hdr->updating == 1) {
		rc = wait_event_interruptible_lock_irq(__zfcp_diag_publish_wait,
						       hdr->updating == 0,
						       hdr->access_lock);
		rc = (rc == 0 ? -EAGAIN : -EINTR);
	} else {
		hdr->updating = 1;
		spin_unlock_irqrestore(&hdr->access_lock, *flags);

		 
		rc = buffer_update(adapter);

		spin_lock_irqsave(&hdr->access_lock, *flags);
		hdr->updating = 0;

		 
		wake_up_interruptible_all(&__zfcp_diag_publish_wait);
	}

	return rc;
}

static bool
__zfcp_diag_test_buffer_age_isfresh(const struct zfcp_diag_adapter *const diag,
				    const struct zfcp_diag_header *const hdr)
	__must_hold(hdr->access_lock)
{
	const unsigned long now = jiffies;

	 
	if (!time_after_eq(now, hdr->timestamp))
		return false;

	if (jiffies_to_msecs(now - hdr->timestamp) >= diag->max_age)
		return false;

	return true;
}

 
int zfcp_diag_update_buffer_limited(struct zfcp_adapter *const adapter,
				    struct zfcp_diag_header *const hdr,
				    zfcp_diag_update_buffer_func buffer_update)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&hdr->access_lock, flags);

	for (rc = 0;
	     !__zfcp_diag_test_buffer_age_isfresh(adapter->diagnostics, hdr);
	     rc = 0) {
		rc = __zfcp_diag_update_buffer(adapter, hdr, buffer_update,
					       &flags);
		if (rc != -EAGAIN)
			break;
	}

	spin_unlock_irqrestore(&hdr->access_lock, flags);

	return rc;
}
