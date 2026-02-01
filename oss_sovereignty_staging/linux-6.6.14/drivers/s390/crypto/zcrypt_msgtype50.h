 
 

#ifndef _ZCRYPT_MSGTYPE50_H_
#define _ZCRYPT_MSGTYPE50_H_

#define MSGTYPE50_NAME			"zcrypt_msgtype50"
#define MSGTYPE50_VARIANT_DEFAULT	0

#define MSGTYPE50_CRB3_MAX_MSG_SIZE 0x710  

#define MSGTYPE_ADJUSTMENT 0x08   

int get_rsa_modex_fc(struct ica_rsa_modexpo *mex, int *fc);
int get_rsa_crt_fc(struct ica_rsa_modexpo_crt *crt, int *fc);

void zcrypt_msgtype50_init(void);
void zcrypt_msgtype50_exit(void);

#endif  
