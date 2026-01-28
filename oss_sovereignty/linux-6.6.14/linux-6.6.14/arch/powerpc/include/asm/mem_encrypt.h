#ifndef _ASM_POWERPC_MEM_ENCRYPT_H
#define _ASM_POWERPC_MEM_ENCRYPT_H
#include <asm/svm.h>
static inline bool force_dma_unencrypted(struct device *dev)
{
	return is_secure_guest();
}
int set_memory_encrypted(unsigned long addr, int numpages);
int set_memory_decrypted(unsigned long addr, int numpages);
#endif  
