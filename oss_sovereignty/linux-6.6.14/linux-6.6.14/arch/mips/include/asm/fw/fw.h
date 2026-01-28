#ifndef __ASM_FW_H_
#define __ASM_FW_H_
#include <asm/bootinfo.h>	 
extern int fw_argc;
extern int *_fw_argv;
extern int *_fw_envp;
#define fw_argv(index)		((char *)(long)_fw_argv[(index)])
#define fw_envp(index)		((char *)(long)_fw_envp[(index)])
extern void fw_init_cmdline(void);
extern char *fw_getcmdline(void);
extern void fw_meminit(void);
extern char *fw_getenv(char *name);
extern unsigned long fw_getenvl(char *name);
extern void fw_init_early_console(void);
#endif  
