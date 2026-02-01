 
 

#ifndef __TPM_FTPM_TEE_H__
#define __TPM_FTPM_TEE_H__

#include <linux/tee_drv.h>
#include <linux/tpm.h>
#include <linux/uuid.h>

 
#define FTPM_OPTEE_TA_SUBMIT_COMMAND  (0)
#define FTPM_OPTEE_TA_EMULATE_PPI     (1)

 
#define MAX_COMMAND_SIZE       4096
#define MAX_RESPONSE_SIZE      4096

 
struct ftpm_tee_private {
	struct tpm_chip *chip;
	u32 session;
	size_t resp_len;
	u8 resp_buf[MAX_RESPONSE_SIZE];
	struct tee_context *ctx;
	struct tee_shm *shm;
};

#endif  
