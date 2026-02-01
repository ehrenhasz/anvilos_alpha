
 

#include <net/sctp/sctp.h>

 
static const char *const sctp_cid_tbl[SCTP_NUM_BASE_CHUNK_TYPES] = {
	"DATA",
	"INIT",
	"INIT_ACK",
	"SACK",
	"HEARTBEAT",
	"HEARTBEAT_ACK",
	"ABORT",
	"SHUTDOWN",
	"SHUTDOWN_ACK",
	"ERROR",
	"COOKIE_ECHO",
	"COOKIE_ACK",
	"ECN_ECNE",
	"ECN_CWR",
	"SHUTDOWN_COMPLETE",
};

 
const char *sctp_cname(const union sctp_subtype cid)
{
	if (cid.chunk <= SCTP_CID_BASE_MAX)
		return sctp_cid_tbl[cid.chunk];

	switch (cid.chunk) {
	case SCTP_CID_ASCONF:
		return "ASCONF";

	case SCTP_CID_ASCONF_ACK:
		return "ASCONF_ACK";

	case SCTP_CID_FWD_TSN:
		return "FWD_TSN";

	case SCTP_CID_AUTH:
		return "AUTH";

	case SCTP_CID_RECONF:
		return "RECONF";

	case SCTP_CID_I_DATA:
		return "I_DATA";

	case SCTP_CID_I_FWD_TSN:
		return "I_FWD_TSN";

	default:
		break;
	}

	return "unknown chunk";
}

 
const char *const sctp_state_tbl[SCTP_STATE_NUM_STATES] = {
	"STATE_CLOSED",
	"STATE_COOKIE_WAIT",
	"STATE_COOKIE_ECHOED",
	"STATE_ESTABLISHED",
	"STATE_SHUTDOWN_PENDING",
	"STATE_SHUTDOWN_SENT",
	"STATE_SHUTDOWN_RECEIVED",
	"STATE_SHUTDOWN_ACK_SENT",
};

 
const char *const sctp_evttype_tbl[] = {
	"EVENT_T_unknown",
	"EVENT_T_CHUNK",
	"EVENT_T_TIMEOUT",
	"EVENT_T_OTHER",
	"EVENT_T_PRIMITIVE"
};

 
const char *const sctp_status_tbl[] = {
	"DISPOSITION_DISCARD",
	"DISPOSITION_CONSUME",
	"DISPOSITION_NOMEM",
	"DISPOSITION_DELETE_TCB",
	"DISPOSITION_ABORT",
	"DISPOSITION_VIOLATION",
	"DISPOSITION_NOT_IMPL",
	"DISPOSITION_ERROR",
	"DISPOSITION_BUG"
};

 
static const char *const sctp_primitive_tbl[SCTP_NUM_PRIMITIVE_TYPES] = {
	"PRIMITIVE_ASSOCIATE",
	"PRIMITIVE_SHUTDOWN",
	"PRIMITIVE_ABORT",
	"PRIMITIVE_SEND",
	"PRIMITIVE_REQUESTHEARTBEAT",
	"PRIMITIVE_ASCONF",
};

 
const char *sctp_pname(const union sctp_subtype id)
{
	if (id.primitive <= SCTP_EVENT_PRIMITIVE_MAX)
		return sctp_primitive_tbl[id.primitive];
	return "unknown_primitive";
}

static const char *const sctp_other_tbl[] = {
	"NO_PENDING_TSN",
	"ICMP_PROTO_UNREACH",
};

 
const char *sctp_oname(const union sctp_subtype id)
{
	if (id.other <= SCTP_EVENT_OTHER_MAX)
		return sctp_other_tbl[id.other];
	return "unknown 'other' event";
}

static const char *const sctp_timer_tbl[] = {
	"TIMEOUT_NONE",
	"TIMEOUT_T1_COOKIE",
	"TIMEOUT_T1_INIT",
	"TIMEOUT_T2_SHUTDOWN",
	"TIMEOUT_T3_RTX",
	"TIMEOUT_T4_RTO",
	"TIMEOUT_T5_SHUTDOWN_GUARD",
	"TIMEOUT_HEARTBEAT",
	"TIMEOUT_RECONF",
	"TIMEOUT_PROBE",
	"TIMEOUT_SACK",
	"TIMEOUT_AUTOCLOSE",
};

 
const char *sctp_tname(const union sctp_subtype id)
{
	BUILD_BUG_ON(SCTP_EVENT_TIMEOUT_MAX + 1 != ARRAY_SIZE(sctp_timer_tbl));

	if (id.timeout < ARRAY_SIZE(sctp_timer_tbl))
		return sctp_timer_tbl[id.timeout];
	return "unknown_timer";
}
