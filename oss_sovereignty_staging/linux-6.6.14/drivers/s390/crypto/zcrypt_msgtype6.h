 
 

#ifndef _ZCRYPT_MSGTYPE6_H_
#define _ZCRYPT_MSGTYPE6_H_

#include <asm/zcrypt.h>

#define MSGTYPE06_NAME			"zcrypt_msgtype6"
#define MSGTYPE06_VARIANT_DEFAULT	0
#define MSGTYPE06_VARIANT_NORNG		1
#define MSGTYPE06_VARIANT_EP11		2

 
struct type6_hdr {
	unsigned char reserved1;	 
	unsigned char type;		 
	unsigned char reserved2[2];	 
	unsigned char right[4];		 
	unsigned char reserved3[2];	 
	unsigned char reserved4[2];	 
	unsigned char apfs[4];		 
	unsigned int  offset1;		 
	unsigned int  offset2;		 
	unsigned int  offset3;		 
	unsigned int  offset4;		 
	unsigned char agent_id[16];	 
					 
	unsigned char rqid[2];		 
	unsigned char reserved5[2];	 
	unsigned char function_code[2];	 
	unsigned char reserved6[2];	 
	unsigned int  tocardlen1;	 
	unsigned int  tocardlen2;	 
	unsigned int  tocardlen3;	 
	unsigned int  tocardlen4;	 
	unsigned int  fromcardlen1;	 
	unsigned int  fromcardlen2;	 
	unsigned int  fromcardlen3;	 
	unsigned int  fromcardlen4;	 
} __packed;

 
struct type86_hdr {
	unsigned char reserved1;	 
	unsigned char type;		 
	unsigned char format;		 
	unsigned char reserved2;	 
	unsigned char reply_code;	 
	unsigned char reserved3[3];	 
} __packed;

#define TYPE86_RSP_CODE 0x86
#define TYPE87_RSP_CODE 0x87
#define TYPE86_FMT2	0x02

struct type86_fmt2_ext {
	unsigned char	  reserved[4];	 
	unsigned char	  apfs[4];	 
	unsigned int	  count1;	 
	unsigned int	  offset1;	 
	unsigned int	  count2;	 
	unsigned int	  offset2;	 
	unsigned int	  count3;	 
	unsigned int	  offset3;	 
	unsigned int	  count4;	 
	unsigned int	  offset4;	 
} __packed;

int prep_cca_ap_msg(bool userspace, struct ica_xcRB *xcrb,
		    struct ap_message *ap_msg,
		    unsigned int *fc, unsigned short **dom);
int prep_ep11_ap_msg(bool userspace, struct ep11_urb *xcrb,
		     struct ap_message *ap_msg,
		     unsigned int *fc, unsigned int *dom);
int prep_rng_ap_msg(struct ap_message *ap_msg,
		    int *fc, unsigned int *dom);

#define LOW	10
#define MEDIUM	100
#define HIGH	500

int speed_idx_cca(int);
int speed_idx_ep11(int);

 
static inline void rng_type6cprb_msgx(struct ap_message *ap_msg,
				      unsigned int random_number_length,
				      unsigned int *domain)
{
	struct {
		struct type6_hdr hdr;
		struct CPRBX cprbx;
		char function_code[2];
		short int rule_length;
		char rule[8];
		short int verb_length;
		short int key_length;
	} __packed * msg = ap_msg->msg;
	static struct type6_hdr static_type6_hdrX = {
		.type		= 0x06,
		.offset1	= 0x00000058,
		.agent_id	= {'C', 'A'},
		.function_code	= {'R', 'L'},
		.tocardlen1	= sizeof(*msg) - sizeof(msg->hdr),
		.fromcardlen1	= sizeof(*msg) - sizeof(msg->hdr),
	};
	static struct CPRBX local_cprbx = {
		.cprb_len	= 0x00dc,
		.cprb_ver_id	= 0x02,
		.func_id	= {0x54, 0x32},
		.req_parml	= sizeof(*msg) - sizeof(msg->hdr) -
				  sizeof(msg->cprbx),
		.rpl_msgbl	= sizeof(*msg) - sizeof(msg->hdr),
	};

	msg->hdr = static_type6_hdrX;
	msg->hdr.fromcardlen2 = random_number_length;
	msg->cprbx = local_cprbx;
	msg->cprbx.rpl_datal = random_number_length;
	memcpy(msg->function_code, msg->hdr.function_code, 0x02);
	msg->rule_length = 0x0a;
	memcpy(msg->rule, "RANDOM  ", 8);
	msg->verb_length = 0x02;
	msg->key_length = 0x02;
	ap_msg->len = sizeof(*msg);
	*domain = (unsigned short)msg->cprbx.domain;
}

void zcrypt_msgtype6_init(void);
void zcrypt_msgtype6_exit(void);

#endif  
