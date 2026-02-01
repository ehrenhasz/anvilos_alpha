 
 

#ifndef OPENSSH_AESCTR_H
#define OPENSSH_AESCTR_H

#include "rijndael.h"

#define AES_BLOCK_SIZE 16

typedef struct aesctr_ctx {
	int	rounds;				 
	u32	ek[4*(AES_MAXROUNDS + 1)];	 
	u8	ctr[AES_BLOCK_SIZE];		 
} aesctr_ctx;

void aesctr_keysetup(aesctr_ctx *x,const u8 *k,u32 kbits,u32 ivbits);
void aesctr_ivsetup(aesctr_ctx *x,const u8 *iv);
void aesctr_encrypt_bytes(aesctr_ctx *x,const u8 *m,u8 *c,u32 bytes);

#endif
