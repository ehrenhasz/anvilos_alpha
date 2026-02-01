 

#include <linux/module.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/isdn/capiutil.h>
#include <linux/slab.h>

#include "kcapi.h"

 

typedef struct {
	int typ;
	size_t off;
} _cdef;

#define _CBYTE	       1
#define _CWORD	       2
#define _CDWORD        3
#define _CSTRUCT       4
#define _CMSTRUCT      5
#define _CEND	       6

static _cdef cdef[] =
{
	 
	{_CEND},
	 
	{_CEND},
	 
	{_CEND},
	 
	{_CDWORD, offsetof(_cmsg, adr.adrController)},
	 
	{_CMSTRUCT, offsetof(_cmsg, AdditionalInfo)},
	 
	{_CSTRUCT, offsetof(_cmsg, B1configuration)},
	 
	{_CWORD, offsetof(_cmsg, B1protocol)},
	 
	{_CSTRUCT, offsetof(_cmsg, B2configuration)},
	 
	{_CWORD, offsetof(_cmsg, B2protocol)},
	 
	{_CSTRUCT, offsetof(_cmsg, B3configuration)},
	 
	{_CWORD, offsetof(_cmsg, B3protocol)},
	 
	{_CSTRUCT, offsetof(_cmsg, BC)},
	 
	{_CSTRUCT, offsetof(_cmsg, BChannelinformation)},
	 
	{_CMSTRUCT, offsetof(_cmsg, BProtocol)},
	 
	{_CSTRUCT, offsetof(_cmsg, CalledPartyNumber)},
	 
	{_CSTRUCT, offsetof(_cmsg, CalledPartySubaddress)},
	 
	{_CSTRUCT, offsetof(_cmsg, CallingPartyNumber)},
	 
	{_CSTRUCT, offsetof(_cmsg, CallingPartySubaddress)},
	 
	{_CDWORD, offsetof(_cmsg, CIPmask)},
	 
	{_CDWORD, offsetof(_cmsg, CIPmask2)},
	 
	{_CWORD, offsetof(_cmsg, CIPValue)},
	 
	{_CDWORD, offsetof(_cmsg, Class)},
	 
	{_CSTRUCT, offsetof(_cmsg, ConnectedNumber)},
	 
	{_CSTRUCT, offsetof(_cmsg, ConnectedSubaddress)},
	 
	{_CDWORD, offsetof(_cmsg, Data)},
	 
	{_CWORD, offsetof(_cmsg, DataHandle)},
	 
	{_CWORD, offsetof(_cmsg, DataLength)},
	 
	{_CSTRUCT, offsetof(_cmsg, FacilityConfirmationParameter)},
	 
	{_CSTRUCT, offsetof(_cmsg, Facilitydataarray)},
	 
	{_CSTRUCT, offsetof(_cmsg, FacilityIndicationParameter)},
	 
	{_CSTRUCT, offsetof(_cmsg, FacilityRequestParameter)},
	 
	{_CWORD, offsetof(_cmsg, FacilitySelector)},
	 
	{_CWORD, offsetof(_cmsg, Flags)},
	 
	{_CDWORD, offsetof(_cmsg, Function)},
	 
	{_CSTRUCT, offsetof(_cmsg, HLC)},
	 
	{_CWORD, offsetof(_cmsg, Info)},
	 
	{_CSTRUCT, offsetof(_cmsg, InfoElement)},
	 
	{_CDWORD, offsetof(_cmsg, InfoMask)},
	 
	{_CWORD, offsetof(_cmsg, InfoNumber)},
	 
	{_CSTRUCT, offsetof(_cmsg, Keypadfacility)},
	 
	{_CSTRUCT, offsetof(_cmsg, LLC)},
	 
	{_CSTRUCT, offsetof(_cmsg, ManuData)},
	 
	{_CDWORD, offsetof(_cmsg, ManuID)},
	 
	{_CSTRUCT, offsetof(_cmsg, NCPI)},
	 
	{_CWORD, offsetof(_cmsg, Reason)},
	 
	{_CWORD, offsetof(_cmsg, Reason_B3)},
	 
	{_CWORD, offsetof(_cmsg, Reject)},
	 
	{_CSTRUCT, offsetof(_cmsg, Useruserdata)}
};

static unsigned char *cpars[] =
{
	  [0x01] = "\x03\x04\x0c\x27\x2f\x1c\x01\x01",
	  [0x02] = "\x03\x14\x0e\x10\x0f\x11\x0d\x06\x08\x0a\x05\x07\x09\x01\x0b\x28\x22\x04\x0c\x27\x2f\x1c\x01\x01",
	  [0x04] = "\x03\x04\x0c\x27\x2f\x1c\x01\x01",
	  [0x05] = "\x03\x25\x12\x13\x10\x11\x01",
	  [0x08] = "\x03\x0e\x04\x0c\x27\x2f\x1c\x01\x01",
	  [0x09] = "\x03\x1f\x1e\x01",
	  [0x0a] = "\x03\x0d\x06\x08\x0a\x05\x07\x09\x01\x01",
	  [0x0b] = "\x03\x2b\x01",
	  [0x0d] = "\x03\x2b\x01",
	  [0x0f] = "\x03\x18\x1a\x19\x20\x01",
	  [0x10] = "\x03\x2b\x01",
	  [0x13] = "\x03\x23\x01",
	  [0x14] = "\x03\x23\x01",
	  [0x16] = "\x03\x23\x01",
	  [0x17] = "\x03\x23\x01",
	  [0x18] = "\x03\x2a\x15\x21\x29\x01",
	  [0x1a] = "\x03\x23\x01",
	  [0x1b] = "\x03\x23\x1f\x1b\x01",
	  [0x1c] = "\x03\x23\x01",
	  [0x1d] = "\x03\x23\x01",
	  [0x1f] = "\x03\x23\x01",
	  [0x21] = "\x03\x19\x23\x01",
	  [0x22] = "\x03\x23\x01",
	  [0x26] = "\x03\x14\x0e\x10\x0f\x11\x0b\x28\x22\x04\x0c\x27\x2f\x1c\x01\x01",
	  [0x27] = "\x03\x16\x17\x28\x01",
	  [0x28] = "\x03\x2c\x01",
	  [0x2a] = "\x03\x2a\x15\x21\x29\x01",
	  [0x2c] = "\x03\x26\x24\x01",
	  [0x2d] = "\x03\x1f\x1d\x01",
	  [0x2f] = "\x03\x2b\x01",
	  [0x30] = "\x03\x2b\x01",
	  [0x31] = "\x03\x2d\x2b\x01",
	  [0x33] = "\x03\x18\x1a\x19\x20\x01",
	  [0x34] = "\x03\x2b\x01",
	  [0x35] = "\x03\x2b\x01",
	  [0x38] = "\x03\x2e\x0d\x06\x08\x0a\x05\x07\x09\x01\x16\x17\x28\x04\x0c\x27\x2f\x1c\x01\x01",
	  [0x39] = "\x03\x01",
	  [0x3a] = "\x03\x01",
	  [0x3c] = "\x03\x2a\x15\x21\x29\x01",
	  [0x3e] = "\x03\x01",
	  [0x3f] = "\x03\x1f\x01",
	  [0x41] = "\x03\x2e\x2b\x01",
	  [0x42] = "\x03\x01",
	  [0x43] = "\x03\x01",
	  [0x45] = "\x03\x19\x01",
	  [0x46] = "\x03\x01",
	  [0x47] = "\x03\x01",
	  [0x4e] = "\x03\x2a\x15\x21\x29\x01",
};

 

#define byteTLcpy(x, y)         *(u8 *)(x) = *(u8 *)(y);
#define wordTLcpy(x, y)         *(u16 *)(x) = *(u16 *)(y);
#define dwordTLcpy(x, y)        memcpy(x, y, 4);
#define structTLcpy(x, y, l)    memcpy(x, y, l)
#define structTLcpyovl(x, y, l) memmove(x, y, l)

#define byteTRcpy(x, y)         *(u8 *)(y) = *(u8 *)(x);
#define wordTRcpy(x, y)         *(u16 *)(y) = *(u16 *)(x);
#define dwordTRcpy(x, y)        memcpy(y, x, 4);
#define structTRcpy(x, y, l)    memcpy(y, x, l)
#define structTRcpyovl(x, y, l) memmove(y, x, l)

 
static unsigned command_2_index(u8 c, u8 sc)
{
	if (c & 0x80)
		c = 0x9 + (c & 0x0f);
	else if (c == 0x41)
		c = 0x9 + 0x1;
	if (c > 0x18)
		c = 0x00;
	return (sc & 3) * (0x9 + 0x9) + c;
}

 

static unsigned char *capi_cmd2par(u8 cmd, u8 subcmd)
{
	return cpars[command_2_index(cmd, subcmd)];
}

 
#define TYP (cdef[cmsg->par[cmsg->p]].typ)
#define OFF (((u8 *)cmsg) + cdef[cmsg->par[cmsg->p]].off)

static void jumpcstruct(_cmsg *cmsg)
{
	unsigned layer;
	for (cmsg->p++, layer = 1; layer;) {
		 
		cmsg->p++;
		switch (TYP) {
		case _CMSTRUCT:
			layer++;
			break;
		case _CEND:
			layer--;
			break;
		}
	}
}

 

static char *mnames[] =
{
	[0x01] = "ALERT_REQ",
	[0x02] = "CONNECT_REQ",
	[0x04] = "DISCONNECT_REQ",
	[0x05] = "LISTEN_REQ",
	[0x08] = "INFO_REQ",
	[0x09] = "FACILITY_REQ",
	[0x0a] = "SELECT_B_PROTOCOL_REQ",
	[0x0b] = "CONNECT_B3_REQ",
	[0x0d] = "DISCONNECT_B3_REQ",
	[0x0f] = "DATA_B3_REQ",
	[0x10] = "RESET_B3_REQ",
	[0x13] = "ALERT_CONF",
	[0x14] = "CONNECT_CONF",
	[0x16] = "DISCONNECT_CONF",
	[0x17] = "LISTEN_CONF",
	[0x18] = "MANUFACTURER_REQ",
	[0x1a] = "INFO_CONF",
	[0x1b] = "FACILITY_CONF",
	[0x1c] = "SELECT_B_PROTOCOL_CONF",
	[0x1d] = "CONNECT_B3_CONF",
	[0x1f] = "DISCONNECT_B3_CONF",
	[0x21] = "DATA_B3_CONF",
	[0x22] = "RESET_B3_CONF",
	[0x26] = "CONNECT_IND",
	[0x27] = "CONNECT_ACTIVE_IND",
	[0x28] = "DISCONNECT_IND",
	[0x2a] = "MANUFACTURER_CONF",
	[0x2c] = "INFO_IND",
	[0x2d] = "FACILITY_IND",
	[0x2f] = "CONNECT_B3_IND",
	[0x30] = "CONNECT_B3_ACTIVE_IND",
	[0x31] = "DISCONNECT_B3_IND",
	[0x33] = "DATA_B3_IND",
	[0x34] = "RESET_B3_IND",
	[0x35] = "CONNECT_B3_T90_ACTIVE_IND",
	[0x38] = "CONNECT_RESP",
	[0x39] = "CONNECT_ACTIVE_RESP",
	[0x3a] = "DISCONNECT_RESP",
	[0x3c] = "MANUFACTURER_IND",
	[0x3e] = "INFO_RESP",
	[0x3f] = "FACILITY_RESP",
	[0x41] = "CONNECT_B3_RESP",
	[0x42] = "CONNECT_B3_ACTIVE_RESP",
	[0x43] = "DISCONNECT_B3_RESP",
	[0x45] = "DATA_B3_RESP",
	[0x46] = "RESET_B3_RESP",
	[0x47] = "CONNECT_B3_T90_ACTIVE_RESP",
	[0x4e] = "MANUFACTURER_RESP"
};

 

char *capi_cmd2str(u8 cmd, u8 subcmd)
{
	char *result;

	result = mnames[command_2_index(cmd, subcmd)];
	if (result == NULL)
		result = "INVALID_COMMAND";
	return result;
}


 

#ifdef CONFIG_CAPI_TRACE

 

static char *pnames[] =
{
	  NULL,
	  NULL,
	  NULL,
	  "Controller/PLCI/NCCI",
	  "AdditionalInfo",
	  "B1configuration",
	  "B1protocol",
	  "B2configuration",
	  "B2protocol",
	  "B3configuration",
	  "B3protocol",
	  "BC",
	  "BChannelinformation",
	  "BProtocol",
	  "CalledPartyNumber",
	  "CalledPartySubaddress",
	  "CallingPartyNumber",
	  "CallingPartySubaddress",
	  "CIPmask",
	  "CIPmask2",
	  "CIPValue",
	  "Class",
	  "ConnectedNumber",
	  "ConnectedSubaddress",
	  "Data32",
	  "DataHandle",
	  "DataLength",
	  "FacilityConfirmationParameter",
	  "Facilitydataarray",
	  "FacilityIndicationParameter",
	  "FacilityRequestParameter",
	  "FacilitySelector",
	  "Flags",
	  "Function",
	  "HLC",
	  "Info",
	  "InfoElement",
	  "InfoMask",
	  "InfoNumber",
	  "Keypadfacility",
	  "LLC",
	  "ManuData",
	  "ManuID",
	  "NCPI",
	  "Reason",
	  "Reason_B3",
	  "Reject",
	  "Useruserdata"
};

#include <linux/stdarg.h>

 
static _cdebbuf *bufprint(_cdebbuf *cdb, char *fmt, ...)
{
	va_list f;
	size_t n, r;

	if (!cdb)
		return NULL;
	va_start(f, fmt);
	r = cdb->size - cdb->pos;
	n = vsnprintf(cdb->p, r, fmt, f);
	va_end(f);
	if (n >= r) {
		 
		size_t ns = 2 * cdb->size;
		u_char *nb;

		while ((ns - cdb->pos) <= n)
			ns *= 2;
		nb = kmalloc(ns, GFP_ATOMIC);
		if (!nb) {
			cdebbuf_free(cdb);
			return NULL;
		}
		memcpy(nb, cdb->buf, cdb->pos);
		kfree(cdb->buf);
		nb[cdb->pos] = 0;
		cdb->buf = nb;
		cdb->p = cdb->buf + cdb->pos;
		cdb->size = ns;
		va_start(f, fmt);
		r = cdb->size - cdb->pos;
		n = vsnprintf(cdb->p, r, fmt, f);
		va_end(f);
	}
	cdb->p += n;
	cdb->pos += n;
	return cdb;
}

static _cdebbuf *printstructlen(_cdebbuf *cdb, u8 *m, unsigned len)
{
	unsigned hex = 0;

	if (!cdb)
		return NULL;
	for (; len; len--, m++)
		if (isalnum(*m) || *m == ' ') {
			if (hex)
				cdb = bufprint(cdb, ">");
			cdb = bufprint(cdb, "%c", *m);
			hex = 0;
		} else {
			if (!hex)
				cdb = bufprint(cdb, "<%02x", *m);
			else
				cdb = bufprint(cdb, " %02x", *m);
			hex = 1;
		}
	if (hex)
		cdb = bufprint(cdb, ">");
	return cdb;
}

static _cdebbuf *printstruct(_cdebbuf *cdb, u8 *m)
{
	unsigned len;

	if (m[0] != 0xff) {
		len = m[0];
		m += 1;
	} else {
		len = ((u16 *) (m + 1))[0];
		m += 3;
	}
	cdb = printstructlen(cdb, m, len);
	return cdb;
}

 
#define NAME (pnames[cmsg->par[cmsg->p]])

static _cdebbuf *protocol_message_2_pars(_cdebbuf *cdb, _cmsg *cmsg, int level)
{
	if (!cmsg->par)
		return NULL;	 

	for (; TYP != _CEND; cmsg->p++) {
		int slen = 29 + 3 - level;
		int i;

		if (!cdb)
			return NULL;
		cdb = bufprint(cdb, "  ");
		for (i = 0; i < level - 1; i++)
			cdb = bufprint(cdb, " ");

		switch (TYP) {
		case _CBYTE:
			cdb = bufprint(cdb, "%-*s = 0x%x\n", slen, NAME, *(u8 *) (cmsg->m + cmsg->l));
			cmsg->l++;
			break;
		case _CWORD:
			cdb = bufprint(cdb, "%-*s = 0x%x\n", slen, NAME, *(u16 *) (cmsg->m + cmsg->l));
			cmsg->l += 2;
			break;
		case _CDWORD:
			cdb = bufprint(cdb, "%-*s = 0x%lx\n", slen, NAME, *(u32 *) (cmsg->m + cmsg->l));
			cmsg->l += 4;
			break;
		case _CSTRUCT:
			cdb = bufprint(cdb, "%-*s = ", slen, NAME);
			if (cmsg->m[cmsg->l] == '\0')
				cdb = bufprint(cdb, "default");
			else
				cdb = printstruct(cdb, cmsg->m + cmsg->l);
			cdb = bufprint(cdb, "\n");
			if (cmsg->m[cmsg->l] != 0xff)
				cmsg->l += 1 + cmsg->m[cmsg->l];
			else
				cmsg->l += 3 + *(u16 *) (cmsg->m + cmsg->l + 1);

			break;

		case _CMSTRUCT:
 
			if (cmsg->m[cmsg->l] == '\0') {
				cdb = bufprint(cdb, "%-*s = default\n", slen, NAME);
				cmsg->l++;
				jumpcstruct(cmsg);
			} else {
				char *name = NAME;
				unsigned _l = cmsg->l;
				cdb = bufprint(cdb, "%-*s\n", slen, name);
				cmsg->l = (cmsg->m + _l)[0] == 255 ? cmsg->l + 3 : cmsg->l + 1;
				cmsg->p++;
				cdb = protocol_message_2_pars(cdb, cmsg, level + 1);
			}
			break;
		}
	}
	return cdb;
}
 

static _cdebbuf *g_debbuf;
static u_long g_debbuf_lock;
static _cmsg *g_cmsg;

static _cdebbuf *cdebbuf_alloc(void)
{
	_cdebbuf *cdb;

	if (likely(!test_and_set_bit(1, &g_debbuf_lock))) {
		cdb = g_debbuf;
		goto init;
	} else
		cdb = kmalloc(sizeof(_cdebbuf), GFP_ATOMIC);
	if (!cdb)
		return NULL;
	cdb->buf = kmalloc(CDEBUG_SIZE, GFP_ATOMIC);
	if (!cdb->buf) {
		kfree(cdb);
		return NULL;
	}
	cdb->size = CDEBUG_SIZE;
init:
	cdb->buf[0] = 0;
	cdb->p = cdb->buf;
	cdb->pos = 0;
	return cdb;
}

 

void cdebbuf_free(_cdebbuf *cdb)
{
	if (likely(cdb == g_debbuf)) {
		test_and_clear_bit(1, &g_debbuf_lock);
		return;
	}
	if (likely(cdb))
		kfree(cdb->buf);
	kfree(cdb);
}


 

_cdebbuf *capi_message2str(u8 *msg)
{
	_cdebbuf *cdb;
	_cmsg	*cmsg;

	cdb = cdebbuf_alloc();
	if (unlikely(!cdb))
		return NULL;
	if (likely(cdb == g_debbuf))
		cmsg = g_cmsg;
	else
		cmsg = kmalloc(sizeof(_cmsg), GFP_ATOMIC);
	if (unlikely(!cmsg)) {
		cdebbuf_free(cdb);
		return NULL;
	}
	cmsg->m = msg;
	cmsg->l = 8;
	cmsg->p = 0;
	byteTRcpy(cmsg->m + 4, &cmsg->Command);
	byteTRcpy(cmsg->m + 5, &cmsg->Subcommand);
	cmsg->par = capi_cmd2par(cmsg->Command, cmsg->Subcommand);

	cdb = bufprint(cdb, "%-26s ID=%03d #0x%04x LEN=%04d\n",
		       capi_cmd2str(cmsg->Command, cmsg->Subcommand),
		       ((unsigned short *) msg)[1],
		       ((unsigned short *) msg)[3],
		       ((unsigned short *) msg)[0]);

	cdb = protocol_message_2_pars(cdb, cmsg, 1);
	if (unlikely(cmsg != g_cmsg))
		kfree(cmsg);
	return cdb;
}

int __init cdebug_init(void)
{
	g_cmsg = kmalloc(sizeof(_cmsg), GFP_KERNEL);
	if (!g_cmsg)
		return -ENOMEM;
	g_debbuf = kmalloc(sizeof(_cdebbuf), GFP_KERNEL);
	if (!g_debbuf) {
		kfree(g_cmsg);
		return -ENOMEM;
	}
	g_debbuf->buf = kmalloc(CDEBUG_GSIZE, GFP_KERNEL);
	if (!g_debbuf->buf) {
		kfree(g_cmsg);
		kfree(g_debbuf);
		return -ENOMEM;
	}
	g_debbuf->size = CDEBUG_GSIZE;
	g_debbuf->buf[0] = 0;
	g_debbuf->p = g_debbuf->buf;
	g_debbuf->pos = 0;
	return 0;
}

void cdebug_exit(void)
{
	if (g_debbuf)
		kfree(g_debbuf->buf);
	kfree(g_debbuf);
	kfree(g_cmsg);
}

#else  

static _cdebbuf g_debbuf = {"CONFIG_CAPI_TRACE not enabled", NULL, 0, 0};

_cdebbuf *capi_message2str(u8 *msg)
{
	return &g_debbuf;
}

_cdebbuf *capi_cmsg2str(_cmsg *cmsg)
{
	return &g_debbuf;
}

void cdebbuf_free(_cdebbuf *cdb)
{
}

int __init cdebug_init(void)
{
	return 0;
}

void cdebug_exit(void)
{
}

#endif
