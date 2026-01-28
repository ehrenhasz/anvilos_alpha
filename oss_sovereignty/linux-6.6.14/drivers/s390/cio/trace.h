#include <linux/kernel.h>
#include <asm/crw.h>
#include <uapi/asm/chpid.h>
#include <uapi/asm/schid.h>
#include "cio.h"
#include "orb.h"
#undef TRACE_SYSTEM
#define TRACE_SYSTEM s390
#if !defined(_TRACE_S390_CIO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_S390_CIO_H
#include <linux/tracepoint.h>
DECLARE_EVENT_CLASS(s390_class_schib,
	TP_PROTO(struct subchannel_id schid, struct schib *schib, int cc),
	TP_ARGS(schid, schib, cc),
	TP_STRUCT__entry(
		__field(u8, cssid)
		__field(u8, ssid)
		__field(u16, schno)
		__field(u16, devno)
		__field_struct(struct schib, schib)
		__field(u8, pmcw_ena)
		__field(u8, pmcw_st)
		__field(u8, pmcw_dnv)
		__field(u16, pmcw_dev)
		__field(u8, pmcw_lpm)
		__field(u8, pmcw_pnom)
		__field(u8, pmcw_lpum)
		__field(u8, pmcw_pim)
		__field(u8, pmcw_pam)
		__field(u8, pmcw_pom)
		__field(u64, pmcw_chpid)
		__field(int, cc)
	),
	TP_fast_assign(
		__entry->cssid = schid.cssid;
		__entry->ssid = schid.ssid;
		__entry->schno = schid.sch_no;
		__entry->devno = schib->pmcw.dev;
		__entry->schib = *schib;
		__entry->pmcw_ena = schib->pmcw.ena;
		__entry->pmcw_st = schib->pmcw.ena;
		__entry->pmcw_dnv = schib->pmcw.dnv;
		__entry->pmcw_dev = schib->pmcw.dev;
		__entry->pmcw_lpm = schib->pmcw.lpm;
		__entry->pmcw_pnom = schib->pmcw.pnom;
		__entry->pmcw_lpum = schib->pmcw.lpum;
		__entry->pmcw_pim = schib->pmcw.pim;
		__entry->pmcw_pam = schib->pmcw.pam;
		__entry->pmcw_pom = schib->pmcw.pom;
		memcpy(&__entry->pmcw_chpid, &schib->pmcw.chpid, 8);
		__entry->cc = cc;
	),
	TP_printk("schid=%x.%x.%04x cc=%d ena=%d st=%d dnv=%d dev=%04x "
		  "lpm=0x%02x pnom=0x%02x lpum=0x%02x pim=0x%02x pam=0x%02x "
		  "pom=0x%02x chpids=%016llx",
		  __entry->cssid, __entry->ssid, __entry->schno, __entry->cc,
		  __entry->pmcw_ena, __entry->pmcw_st,
		  __entry->pmcw_dnv, __entry->pmcw_dev,
		  __entry->pmcw_lpm, __entry->pmcw_pnom,
		  __entry->pmcw_lpum, __entry->pmcw_pim,
		  __entry->pmcw_pam, __entry->pmcw_pom,
		  __entry->pmcw_chpid
	)
);
DEFINE_EVENT(s390_class_schib, s390_cio_stsch,
	TP_PROTO(struct subchannel_id schid, struct schib *schib, int cc),
	TP_ARGS(schid, schib, cc)
);
DEFINE_EVENT(s390_class_schib, s390_cio_msch,
	TP_PROTO(struct subchannel_id schid, struct schib *schib, int cc),
	TP_ARGS(schid, schib, cc)
);
TRACE_EVENT(s390_cio_tsch,
	TP_PROTO(struct subchannel_id schid, struct irb *irb, int cc),
	TP_ARGS(schid, irb, cc),
	TP_STRUCT__entry(
		__field(u8, cssid)
		__field(u8, ssid)
		__field(u16, schno)
		__field_struct(struct irb, irb)
		__field(u8, scsw_dcc)
		__field(u8, scsw_pno)
		__field(u8, scsw_fctl)
		__field(u8, scsw_actl)
		__field(u8, scsw_stctl)
		__field(u8, scsw_dstat)
		__field(u8, scsw_cstat)
		__field(int, cc)
	),
	TP_fast_assign(
		__entry->cssid = schid.cssid;
		__entry->ssid = schid.ssid;
		__entry->schno = schid.sch_no;
		__entry->irb = *irb;
		__entry->scsw_dcc = scsw_cc(&irb->scsw);
		__entry->scsw_pno = scsw_pno(&irb->scsw);
		__entry->scsw_fctl = scsw_fctl(&irb->scsw);
		__entry->scsw_actl = scsw_actl(&irb->scsw);
		__entry->scsw_stctl = scsw_stctl(&irb->scsw);
		__entry->scsw_dstat = scsw_dstat(&irb->scsw);
		__entry->scsw_cstat = scsw_cstat(&irb->scsw);
		__entry->cc = cc;
	),
	TP_printk("schid=%x.%x.%04x cc=%d dcc=%d pno=%d fctl=0x%x actl=0x%x "
		  "stctl=0x%x dstat=0x%x cstat=0x%x",
		  __entry->cssid, __entry->ssid, __entry->schno, __entry->cc,
		  __entry->scsw_dcc, __entry->scsw_pno,
		  __entry->scsw_fctl, __entry->scsw_actl,
		  __entry->scsw_stctl,
		  __entry->scsw_dstat, __entry->scsw_cstat
	)
);
TRACE_EVENT(s390_cio_tpi,
	TP_PROTO(struct tpi_info *addr, int cc),
	TP_ARGS(addr, cc),
	TP_STRUCT__entry(
		__field(int, cc)
		__field_struct(struct tpi_info, tpi_info)
		__field(u8, cssid)
		__field(u8, ssid)
		__field(u16, schno)
		__field(u8, adapter_IO)
		__field(u8, isc)
		__field(u8, type)
	),
	TP_fast_assign(
		__entry->cc = cc;
		if (cc != 0)
			memset(&__entry->tpi_info, 0, sizeof(struct tpi_info));
		else if (addr)
			__entry->tpi_info = *addr;
		else
			__entry->tpi_info = S390_lowcore.tpi_info;
		__entry->cssid = __entry->tpi_info.schid.cssid;
		__entry->ssid = __entry->tpi_info.schid.ssid;
		__entry->schno = __entry->tpi_info.schid.sch_no;
		__entry->adapter_IO = __entry->tpi_info.adapter_IO;
		__entry->isc = __entry->tpi_info.isc;
		__entry->type = __entry->tpi_info.type;
	),
	TP_printk("schid=%x.%x.%04x cc=%d a=%d isc=%d type=%d",
		  __entry->cssid, __entry->ssid, __entry->schno, __entry->cc,
		  __entry->adapter_IO, __entry->isc,
		  __entry->type
	)
);
TRACE_EVENT(s390_cio_ssch,
	TP_PROTO(struct subchannel_id schid, union orb *orb, int cc),
	TP_ARGS(schid, orb, cc),
	TP_STRUCT__entry(
		__field(u8, cssid)
		__field(u8, ssid)
		__field(u16, schno)
		__field_struct(union orb, orb)
		__field(int, cc)
	),
	TP_fast_assign(
		__entry->cssid = schid.cssid;
		__entry->ssid = schid.ssid;
		__entry->schno = schid.sch_no;
		__entry->orb = *orb;
		__entry->cc = cc;
	),
	TP_printk("schid=%x.%x.%04x cc=%d", __entry->cssid, __entry->ssid,
		  __entry->schno, __entry->cc
	)
);
DECLARE_EVENT_CLASS(s390_class_schid,
	TP_PROTO(struct subchannel_id schid, int cc),
	TP_ARGS(schid, cc),
	TP_STRUCT__entry(
		__field(u8, cssid)
		__field(u8, ssid)
		__field(u16, schno)
		__field(int, cc)
	),
	TP_fast_assign(
		__entry->cssid = schid.cssid;
		__entry->ssid = schid.ssid;
		__entry->schno = schid.sch_no;
		__entry->cc = cc;
	),
	TP_printk("schid=%x.%x.%04x cc=%d", __entry->cssid, __entry->ssid,
		  __entry->schno, __entry->cc
	)
);
DEFINE_EVENT(s390_class_schid, s390_cio_csch,
	TP_PROTO(struct subchannel_id schid, int cc),
	TP_ARGS(schid, cc)
);
DEFINE_EVENT(s390_class_schid, s390_cio_hsch,
	TP_PROTO(struct subchannel_id schid, int cc),
	TP_ARGS(schid, cc)
);
DEFINE_EVENT(s390_class_schid, s390_cio_xsch,
	TP_PROTO(struct subchannel_id schid, int cc),
	TP_ARGS(schid, cc)
);
DEFINE_EVENT(s390_class_schid, s390_cio_rsch,
	TP_PROTO(struct subchannel_id schid, int cc),
	TP_ARGS(schid, cc)
);
#define CHSC_MAX_REQUEST_LEN		64
#define CHSC_MAX_RESPONSE_LEN		64
TRACE_EVENT(s390_cio_chsc,
	TP_PROTO(struct chsc_header *chsc, int cc),
	TP_ARGS(chsc, cc),
	TP_STRUCT__entry(
		__field(int, cc)
		__field(u16, code)
		__field(u16, rcode)
		__array(u8, request, CHSC_MAX_REQUEST_LEN)
		__array(u8, response, CHSC_MAX_RESPONSE_LEN)
	),
	TP_fast_assign(
		__entry->cc = cc;
		__entry->code = chsc->code;
		memcpy(&entry->request, chsc,
		       min_t(u16, chsc->length, CHSC_MAX_REQUEST_LEN));
		chsc = (struct chsc_header *) ((char *) chsc + chsc->length);
		__entry->rcode = chsc->code;
		memcpy(&entry->response, chsc,
		       min_t(u16, chsc->length, CHSC_MAX_RESPONSE_LEN));
	),
	TP_printk("code=0x%04x cc=%d rcode=0x%04x", __entry->code,
		  __entry->cc, __entry->rcode)
);
TRACE_EVENT(s390_cio_interrupt,
	TP_PROTO(struct tpi_info *tpi_info),
	TP_ARGS(tpi_info),
	TP_STRUCT__entry(
		__field_struct(struct tpi_info, tpi_info)
		__field(u8, cssid)
		__field(u8, ssid)
		__field(u16, schno)
		__field(u8, isc)
		__field(u8, type)
	),
	TP_fast_assign(
		__entry->tpi_info = *tpi_info;
		__entry->cssid = tpi_info->schid.cssid;
		__entry->ssid = tpi_info->schid.ssid;
		__entry->schno = tpi_info->schid.sch_no;
		__entry->isc = tpi_info->isc;
		__entry->type = tpi_info->type;
	),
	TP_printk("schid=%x.%x.%04x isc=%d type=%d",
		  __entry->cssid, __entry->ssid, __entry->schno,
		  __entry->isc, __entry->type
	)
);
TRACE_EVENT(s390_cio_adapter_int,
	TP_PROTO(struct tpi_info *tpi_info),
	TP_ARGS(tpi_info),
	TP_STRUCT__entry(
		__field_struct(struct tpi_info, tpi_info)
		__field(u8, isc)
	),
	TP_fast_assign(
		__entry->tpi_info = *tpi_info;
		__entry->isc = tpi_info->isc;
	),
	TP_printk("isc=%d", __entry->isc)
);
TRACE_EVENT(s390_cio_stcrw,
	TP_PROTO(struct crw *crw, int cc),
	TP_ARGS(crw, cc),
	TP_STRUCT__entry(
		__field_struct(struct crw, crw)
		__field(int, cc)
		__field(u8, slct)
		__field(u8, oflw)
		__field(u8, chn)
		__field(u8, rsc)
		__field(u8, anc)
		__field(u8, erc)
		__field(u16, rsid)
	),
	TP_fast_assign(
		__entry->crw = *crw;
		__entry->cc = cc;
		__entry->slct = crw->slct;
		__entry->oflw = crw->oflw;
		__entry->chn = crw->chn;
		__entry->rsc = crw->rsc;
		__entry->anc = crw->anc;
		__entry->erc = crw->erc;
		__entry->rsid = crw->rsid;
	),
	TP_printk("cc=%d slct=%d oflw=%d chn=%d rsc=%d anc=%d erc=0x%x "
		  "rsid=0x%x",
		  __entry->cc, __entry->slct, __entry->oflw,
		  __entry->chn, __entry->rsc,  __entry->anc,
		  __entry->erc, __entry->rsid
	)
);
#endif  
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
