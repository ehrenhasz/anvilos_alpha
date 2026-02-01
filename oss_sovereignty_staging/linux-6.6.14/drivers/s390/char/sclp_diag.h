 
 

#ifndef _SCLP_DIAG_H
#define _SCLP_DIAG_H

#include <linux/types.h>

 
#define SCLP_DIAG_FTP_OK	0x80U  
#define SCLP_DIAG_FTP_LDFAIL	0x01U  
#define SCLP_DIAG_FTP_LDNPERM	0x02U  
#define SCLP_DIAG_FTP_LDRUNS	0x03U  
#define SCLP_DIAG_FTP_LDNRUNS	0x04U  

#define SCLP_DIAG_FTP_XPCX	0x80  
#define SCLP_DIAG_FTP_ROUTE	4  

 
#define SCLP_DIAG_FTP_EVBUF_LEN				\
	(offsetof(struct sclp_diag_evbuf, mdd) +	\
	 sizeof(struct sclp_diag_ftp))

 
struct sclp_diag_ftp {
	u8 pcx;
	u8 ldflg;
	u8 cmd;
	u8 pgsize;
	u8 srcflg;
	u8 spare;
	u64 offset;
	u64 fsize;
	u64 length;
	u64 failaddr;
	u64 bufaddr;
	u64 asce;

	u8 fident[256];
} __packed;

 
struct sclp_diag_evbuf {
	struct evbuf_header hdr;
	u16 route;

	union {
		struct sclp_diag_ftp ftp;
	} mdd;
} __packed;

 
struct sclp_diag_sccb {

	struct sccb_header hdr;
	struct sclp_diag_evbuf evbuf;
} __packed;

#endif  
