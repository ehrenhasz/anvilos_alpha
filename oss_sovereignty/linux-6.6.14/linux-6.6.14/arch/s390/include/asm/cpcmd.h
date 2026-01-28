#ifndef _ASM_S390_CPCMD_H
#define _ASM_S390_CPCMD_H
int __cpcmd(const char *cmd, char *response, int rlen, int *response_code);
int cpcmd(const char *cmd, char *response, int rlen, int *response_code);
#endif  
