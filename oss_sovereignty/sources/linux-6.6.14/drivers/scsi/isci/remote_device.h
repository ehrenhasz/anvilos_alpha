

#ifndef _ISCI_REMOTE_DEVICE_H_
#define _ISCI_REMOTE_DEVICE_H_
#include <scsi/libsas.h>
#include <linux/kref.h>
#include "scu_remote_node_context.h"
#include "remote_node_context.h"
#include "port.h"

enum sci_remote_device_not_ready_reason_code {
	SCIC_REMOTE_DEVICE_NOT_READY_START_REQUESTED,
	SCIC_REMOTE_DEVICE_NOT_READY_STOP_REQUESTED,
	SCIC_REMOTE_DEVICE_NOT_READY_SATA_REQUEST_STARTED,
	SCIC_REMOTE_DEVICE_NOT_READY_SATA_SDB_ERROR_FIS_RECEIVED,
	SCIC_REMOTE_DEVICE_NOT_READY_SMP_REQUEST_STARTED,
	SCIC_REMOTE_DEVICE_NOT_READY_REASON_CODE_MAX
};


struct isci_remote_device {
	#define IDEV_START_PENDING 0
	#define IDEV_STOP_PENDING 1
	#define IDEV_ALLOCATED 2
	#define IDEV_GONE 3
	#define IDEV_IO_READY 4
	#define IDEV_IO_NCQERROR 5
	#define IDEV_RNC_LLHANG_ENABLED 6
	#define IDEV_ABORT_PATH_ACTIVE 7
	#define IDEV_ABORT_PATH_RESUME_PENDING 8
	unsigned long flags;
	struct kref kref;
	struct isci_port *isci_port;
	struct domain_device *domain_dev;
	struct list_head node;
	struct sci_base_state_machine sm;
	u32 device_port_width;
	enum sas_linkrate connection_rate;
	struct isci_port *owning_port;
	struct sci_remote_node_context rnc;
	
	u32 started_request_count;
	struct isci_request *working_request;
	u32 not_ready_reason;
	scics_sds_remote_node_context_callback abort_resume_cb;
	void *abort_resume_cbparam;
};

#define ISCI_REMOTE_DEVICE_START_TIMEOUT 5000


static inline struct isci_remote_device *isci_get_device(
	struct isci_remote_device *idev)
{
	if (idev)
		kref_get(&idev->kref);
	return idev;
}

static inline struct isci_remote_device *isci_lookup_device(struct domain_device *dev)
{
	struct isci_remote_device *idev = dev->lldd_dev;

	if (idev && !test_bit(IDEV_GONE, &idev->flags)) {
		kref_get(&idev->kref);
		return idev;
	}

	return NULL;
}

void isci_remote_device_release(struct kref *kref);
static inline void isci_put_device(struct isci_remote_device *idev)
{
	if (idev)
		kref_put(&idev->kref, isci_remote_device_release);
}

enum sci_status isci_remote_device_stop(struct isci_host *ihost,
					struct isci_remote_device *idev);
void isci_remote_device_nuke_requests(struct isci_host *ihost,
				      struct isci_remote_device *idev);
void isci_remote_device_gone(struct domain_device *domain_dev);
int isci_remote_device_found(struct domain_device *domain_dev);


enum sci_status sci_remote_device_stop(
	struct isci_remote_device *idev,
	u32 timeout);


enum sci_status sci_remote_device_reset(
	struct isci_remote_device *idev);


enum sci_status sci_remote_device_reset_complete(
	struct isci_remote_device *idev);


#define REMOTE_DEV_STATES {\
	C(DEV_INITIAL),\
	C(DEV_STOPPED),\
	C(DEV_STARTING),\
	C(DEV_READY),\
	C(STP_DEV_IDLE),\
	C(STP_DEV_CMD),\
	C(STP_DEV_NCQ),\
	C(STP_DEV_NCQ_ERROR),\
	C(STP_DEV_ATAPI_ERROR),\
	C(STP_DEV_AWAIT_RESET),\
	C(SMP_DEV_IDLE),\
	C(SMP_DEV_CMD),\
	C(DEV_STOPPING),\
	C(DEV_FAILED),\
	C(DEV_RESETTING),\
	C(DEV_FINAL),\
	}
#undef C
#define C(a) SCI_##a
enum sci_remote_device_states REMOTE_DEV_STATES;
#undef C
const char *dev_state_name(enum sci_remote_device_states state);

static inline struct isci_remote_device *rnc_to_dev(struct sci_remote_node_context *rnc)
{
	struct isci_remote_device *idev;

	idev = container_of(rnc, typeof(*idev), rnc);

	return idev;
}

static inline void sci_remote_device_decrement_request_count(struct isci_remote_device *idev)
{
	
	if (WARN_ONCE(idev->started_request_count == 0,
		      "%s: tried to decrement started_request_count past 0!?",
			__func__))
		;
	else
		idev->started_request_count--;
}

void isci_dev_set_hang_detection_timeout(struct isci_remote_device *idev, u32 timeout);

enum sci_status sci_remote_device_frame_handler(
	struct isci_remote_device *idev,
	u32 frame_index);

enum sci_status sci_remote_device_event_handler(
	struct isci_remote_device *idev,
	u32 event_code);

enum sci_status sci_remote_device_start_io(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status sci_remote_device_start_task(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status sci_remote_device_complete_io(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

void sci_remote_device_post_request(
	struct isci_remote_device *idev,
	u32 request);

enum sci_status sci_remote_device_terminate_requests(
	struct isci_remote_device *idev);

int isci_remote_device_is_safe_to_abort(
	struct isci_remote_device *idev);

enum sci_status
sci_remote_device_abort_requests_pending_abort(
	struct isci_remote_device *idev);

enum sci_status isci_remote_device_suspend(
	struct isci_host *ihost,
	struct isci_remote_device *idev);

enum sci_status sci_remote_device_resume(
	struct isci_remote_device *idev,
	scics_sds_remote_node_context_callback cb_fn,
	void *cb_p);

enum sci_status isci_remote_device_resume_from_abort(
	struct isci_host *ihost,
	struct isci_remote_device *idev);

enum sci_status isci_remote_device_reset(
	struct isci_host *ihost,
	struct isci_remote_device *idev);

enum sci_status isci_remote_device_reset_complete(
	struct isci_host *ihost,
	struct isci_remote_device *idev);

enum sci_status isci_remote_device_suspend_terminate(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);

enum sci_status isci_remote_device_terminate_requests(
	struct isci_host *ihost,
	struct isci_remote_device *idev,
	struct isci_request *ireq);
enum sci_status sci_remote_device_suspend(struct isci_remote_device *idev,
					  enum sci_remote_node_suspension_reasons reason);
#endif 
