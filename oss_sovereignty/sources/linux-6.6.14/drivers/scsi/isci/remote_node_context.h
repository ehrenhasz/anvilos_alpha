

#ifndef _SCIC_SDS_REMOTE_NODE_CONTEXT_H_
#define _SCIC_SDS_REMOTE_NODE_CONTEXT_H_



#include "isci.h"


#define SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX    0x0FFF

enum sci_remote_node_suspension_reasons {
	SCI_HW_SUSPEND,
	SCI_SW_SUSPEND_NORMAL,
	SCI_SW_SUSPEND_LINKHANG_DETECT
};
#define SCI_SOFTWARE_SUSPEND_CMD SCU_CONTEXT_COMMAND_POST_RNC_SUSPEND_TX_RX
#define SCI_SOFTWARE_SUSPEND_EXPECTED_EVENT SCU_EVENT_TL_RNC_SUSPEND_TX_RX

struct isci_request;
struct isci_remote_device;
struct sci_remote_node_context;

typedef void (*scics_sds_remote_node_context_callback)(void *);


#define RNC_STATES {\
	C(RNC_INITIAL),\
	C(RNC_POSTING),\
	C(RNC_INVALIDATING),\
	C(RNC_RESUMING),\
	C(RNC_READY),\
	C(RNC_TX_SUSPENDED),\
	C(RNC_TX_RX_SUSPENDED),\
	C(RNC_AWAIT_SUSPENSION),\
	}
#undef C
#define C(a) SCI_##a
enum scis_sds_remote_node_context_states RNC_STATES;
#undef C
const char *rnc_state_name(enum scis_sds_remote_node_context_states state);


enum sci_remote_node_context_destination_state {
	RNC_DEST_UNSPECIFIED,
	RNC_DEST_READY,
	RNC_DEST_FINAL,
	RNC_DEST_SUSPENDED,       
	RNC_DEST_SUSPENDED_RESUME 
};


struct sci_remote_node_context {
	
	u16 remote_node_index;

	
	u32 suspend_type;
	enum sci_remote_node_suspension_reasons suspend_reason;
	u32 suspend_count;

	
	enum sci_remote_node_context_destination_state destination_state;

	
	scics_sds_remote_node_context_callback user_callback;

	
	void *user_cookie;

	
	struct sci_base_state_machine sm;
};

void sci_remote_node_context_construct(struct sci_remote_node_context *rnc,
					    u16 remote_node_index);


bool sci_remote_node_context_is_ready(
	struct sci_remote_node_context *sci_rnc);

bool sci_remote_node_context_is_suspended(struct sci_remote_node_context *sci_rnc);

enum sci_status sci_remote_node_context_event_handler(struct sci_remote_node_context *sci_rnc,
							   u32 event_code);
enum sci_status sci_remote_node_context_destruct(struct sci_remote_node_context *sci_rnc,
						      scics_sds_remote_node_context_callback callback,
						      void *callback_parameter);
enum sci_status sci_remote_node_context_suspend(struct sci_remote_node_context *sci_rnc,
						     enum sci_remote_node_suspension_reasons reason,
						     u32 suspension_code);
enum sci_status sci_remote_node_context_resume(struct sci_remote_node_context *sci_rnc,
						    scics_sds_remote_node_context_callback cb_fn,
						    void *cb_p);
enum sci_status sci_remote_node_context_start_task(struct sci_remote_node_context *sci_rnc,
						   struct isci_request *ireq,
						   scics_sds_remote_node_context_callback cb_fn,
						   void *cb_p);
enum sci_status sci_remote_node_context_start_io(struct sci_remote_node_context *sci_rnc,
						      struct isci_request *ireq);
int sci_remote_node_context_is_safe_to_abort(
	struct sci_remote_node_context *sci_rnc);

static inline bool sci_remote_node_context_is_being_destroyed(
	struct sci_remote_node_context *sci_rnc)
{
	return (sci_rnc->destination_state == RNC_DEST_FINAL)
		|| ((sci_rnc->sm.current_state_id == SCI_RNC_INITIAL)
		    && (sci_rnc->destination_state == RNC_DEST_UNSPECIFIED));
}
#endif  
