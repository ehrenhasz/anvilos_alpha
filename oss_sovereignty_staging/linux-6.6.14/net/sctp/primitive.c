
 

#include <linux/types.h>
#include <linux/list.h>  
#include <linux/socket.h>
#include <linux/ip.h>
#include <linux/time.h>  
#include <linux/gfp.h>
#include <net/sock.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

#define DECLARE_PRIMITIVE(name) \
  \
int sctp_primitive_ ## name(struct net *net, struct sctp_association *asoc, \
			    void *arg) { \
	int error = 0; \
	enum sctp_event_type event_type; union sctp_subtype subtype; \
	enum sctp_state state; \
	struct sctp_endpoint *ep; \
	\
	event_type = SCTP_EVENT_T_PRIMITIVE; \
	subtype = SCTP_ST_PRIMITIVE(SCTP_PRIMITIVE_ ## name); \
	state = asoc ? asoc->state : SCTP_STATE_CLOSED; \
	ep = asoc ? asoc->ep : NULL; \
	\
	error = sctp_do_sm(net, event_type, subtype, state, ep, asoc,	\
			   arg, GFP_KERNEL); \
	return error; \
}

 

 

DECLARE_PRIMITIVE(ASSOCIATE)

 

DECLARE_PRIMITIVE(SHUTDOWN);

 

DECLARE_PRIMITIVE(ABORT);

 

DECLARE_PRIMITIVE(SEND);

 

DECLARE_PRIMITIVE(REQUESTHEARTBEAT);

 

DECLARE_PRIMITIVE(ASCONF);

 
DECLARE_PRIMITIVE(RECONF);
