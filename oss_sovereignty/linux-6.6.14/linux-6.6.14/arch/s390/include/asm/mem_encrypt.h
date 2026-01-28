#ifndef S390_MEM_ENCRYPT_H__
#define S390_MEM_ENCRYPT_H__
#ifndef __ASSEMBLY__
int set_memory_encrypted(unsigned long vaddr, int numpages);
int set_memory_decrypted(unsigned long vaddr, int numpages);
#endif	 
#endif	 
