#ifndef _ISCI_REQUEST_H_
#define _ISCI_REQUEST_H_
#include "isci.h"
#include "host.h"
#include "scu_task_context.h"
struct isci_stp_request {
	u32 pio_len;
	u8 status;
	struct isci_stp_pio_sgl {
		int index;
		u8 set;
		u32 offset;
	} sgl;
};
struct isci_request {
	#define IREQ_COMPLETE_IN_TARGET 0
	#define IREQ_TERMINATED 1
	#define IREQ_TMF 2
	#define IREQ_ACTIVE 3
	#define IREQ_PENDING_ABORT 4  
	#define IREQ_TC_ABORT_POSTED 5
	#define IREQ_ABORT_PATH_ACTIVE 6
	#define IREQ_NO_AUTO_FREE_TAG 7  
	unsigned long flags;
	union ttype_ptr_union {
		struct sas_task *io_task_ptr;    
		struct isci_tmf *tmf_task_ptr;   
	} ttype_ptr;
	struct isci_host *isci_host;
	dma_addr_t request_daddr;
	dma_addr_t zero_scatter_daddr;
	unsigned int num_sg_entries;
	struct completion *io_request_completion;
	struct sci_base_state_machine sm;
	struct isci_host *owning_controller;
	struct isci_remote_device *target_device;
	u16 io_tag;
	enum sas_protocol protocol;
	u32 scu_status;  
	u32 sci_status;  
	u32 post_context;
	struct scu_task_context *tc;
	#define SCU_SGL_SIZE ((SCI_MAX_SCATTER_GATHER_ELEMENTS + 1) / 2)
	struct scu_sgl_element_pair sg_table[SCU_SGL_SIZE] __attribute__ ((aligned(32)));
	u32 saved_rx_frame_index;
	union {
		struct {
			union {
				struct ssp_cmd_iu cmd;
				struct ssp_task_iu tmf;
			};
			union {
				struct ssp_response_iu rsp;
				u8 rsp_buf[SSP_RESP_IU_MAX_SIZE];
			};
		} ssp;
		struct {
			struct isci_stp_request req;
			struct host_to_dev_fis cmd;
			struct dev_to_host_fis rsp;
		} stp;
	};
};
static inline struct isci_request *to_ireq(struct isci_stp_request *stp_req)
{
	struct isci_request *ireq;
	ireq = container_of(stp_req, typeof(*ireq), stp.req);
	return ireq;
}
#define REQUEST_STATES {\
	C(REQ_INIT),\
	C(REQ_CONSTRUCTED),\
	C(REQ_STARTED),\
	C(REQ_STP_UDMA_WAIT_TC_COMP),\
	C(REQ_STP_UDMA_WAIT_D2H),\
	C(REQ_STP_NON_DATA_WAIT_H2D),\
	C(REQ_STP_NON_DATA_WAIT_D2H),\
	C(REQ_STP_PIO_WAIT_H2D),\
	C(REQ_STP_PIO_WAIT_FRAME),\
	C(REQ_STP_PIO_DATA_IN),\
	C(REQ_STP_PIO_DATA_OUT),\
	C(REQ_ATAPI_WAIT_H2D),\
	C(REQ_ATAPI_WAIT_PIO_SETUP),\
	C(REQ_ATAPI_WAIT_D2H),\
	C(REQ_ATAPI_WAIT_TC_COMP),\
	C(REQ_TASK_WAIT_TC_COMP),\
	C(REQ_TASK_WAIT_TC_RESP),\
	C(REQ_SMP_WAIT_RESP),\
	C(REQ_SMP_WAIT_TC_COMP),\
	C(REQ_COMPLETED),\
	C(REQ_ABORTING),\
	C(REQ_FINAL),\
	}
#undef C
#define C(a) SCI_##a
enum sci_base_request_states REQUEST_STATES;
#undef C
const char *req_state_name(enum sci_base_request_states state);
enum sci_status sci_request_start(struct isci_request *ireq);
enum sci_status sci_io_request_terminate(struct isci_request *ireq);
enum sci_status
sci_io_request_event_handler(struct isci_request *ireq,
				  u32 event_code);
enum sci_status
sci_io_request_frame_handler(struct isci_request *ireq,
				  u32 frame_index);
enum sci_status
sci_task_request_terminate(struct isci_request *ireq);
extern enum sci_status
sci_request_complete(struct isci_request *ireq);
extern enum sci_status
sci_io_request_tc_completion(struct isci_request *ireq, u32 code);
static inline dma_addr_t
sci_io_request_get_dma_addr(struct isci_request *ireq, void *virt_addr)
{
	char *requested_addr = (char *)virt_addr;
	char *base_addr = (char *)ireq;
	BUG_ON(requested_addr < base_addr);
	BUG_ON((requested_addr - base_addr) >= sizeof(*ireq));
	return ireq->request_daddr + (requested_addr - base_addr);
}
#define isci_request_access_task(req) ((req)->ttype_ptr.io_task_ptr)
#define isci_request_access_tmf(req) ((req)->ttype_ptr.tmf_task_ptr)
struct isci_request *isci_tmf_request_from_tag(struct isci_host *ihost,
					       struct isci_tmf *isci_tmf,
					       u16 tag);
int isci_request_execute(struct isci_host *ihost, struct isci_remote_device *idev,
			 struct sas_task *task, struct isci_request *ireq);
struct isci_request *isci_io_request_from_tag(struct isci_host *ihost,
					      struct sas_task *task,
					      u16 tag);
enum sci_status
sci_task_request_construct(struct isci_host *ihost,
			    struct isci_remote_device *idev,
			    u16 io_tag,
			    struct isci_request *ireq);
enum sci_status sci_task_request_construct_ssp(struct isci_request *ireq);
void sci_smp_request_copy_response(struct isci_request *ireq);
static inline int isci_task_is_ncq_recovery(struct sas_task *task)
{
	return (sas_protocol_ata(task->task_proto) &&
		task->ata_task.fis.command == ATA_CMD_READ_LOG_EXT &&
		task->ata_task.fis.lbal == ATA_LOG_SATA_NCQ);
}
#endif  
