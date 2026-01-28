#undef TRACE_SYSTEM
#define TRACE_SYSTEM s390
#if !defined(_TRACE_S390_ZCRYPT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_S390_ZCRYPT_H
#include <linux/tracepoint.h>
#define TP_ICARSAMODEXPO  0x0001
#define TP_ICARSACRT	  0x0002
#define TB_ZSECSENDCPRB   0x0003
#define TP_ZSENDEP11CPRB  0x0004
#define TP_HWRNGCPRB	  0x0005
#define show_zcrypt_tp_type(type)				\
	__print_symbolic(type,					\
			 { TP_ICARSAMODEXPO, "ICARSAMODEXPO" }, \
			 { TP_ICARSACRT, "ICARSACRT" },		\
			 { TB_ZSECSENDCPRB, "ZSECSENDCPRB" },	\
			 { TP_ZSENDEP11CPRB, "ZSENDEP11CPRB" }, \
			 { TP_HWRNGCPRB, "HWRNGCPRB" })
TRACE_EVENT(s390_zcrypt_req,
	    TP_PROTO(void *ptr, u32 type),
	    TP_ARGS(ptr, type),
	    TP_STRUCT__entry(
		    __field(void *, ptr)
		    __field(u32, type)),
	    TP_fast_assign(
		    __entry->ptr = ptr;
		    __entry->type = type;),
	    TP_printk("ptr=%p type=%s",
		      __entry->ptr,
		      show_zcrypt_tp_type(__entry->type))
);
TRACE_EVENT(s390_zcrypt_rep,
	    TP_PROTO(void *ptr, u32 fc, u32 rc, u16 dev, u16 dom),
	    TP_ARGS(ptr, fc, rc, dev, dom),
	    TP_STRUCT__entry(
		    __field(void *, ptr)
		    __field(u32, fc)
		    __field(u32, rc)
		    __field(u16, device)
		    __field(u16, domain)),
	    TP_fast_assign(
		    __entry->ptr = ptr;
		    __entry->fc = fc;
		    __entry->rc = rc;
		    __entry->device = dev;
		    __entry->domain = dom;),
	    TP_printk("ptr=%p fc=0x%04x rc=%d dev=0x%02hx domain=0x%04hx",
		      __entry->ptr,
		      (unsigned int) __entry->fc,
		      (int) __entry->rc,
		      (unsigned short) __entry->device,
		      (unsigned short) __entry->domain)
);
#endif  
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH asm/trace
#define TRACE_INCLUDE_FILE zcrypt
#include <trace/define_trace.h>
