#ifndef _ASM_SGIALIB_H
#define _ASM_SGIALIB_H
#include <linux/compiler.h>
#include <asm/sgiarcs.h>
extern struct linux_romvec *romvec;
extern int prom_flags;
#define PROM_FLAG_ARCS			1
#define PROM_FLAG_USE_AS_CONSOLE	2
#define PROM_FLAG_DONT_FREE_TEMP	4
extern char prom_getchar(void);
extern struct linux_mdesc *prom_getmdesc(struct linux_mdesc *curr);
#define PROM_NULL_MDESC	  ((struct linux_mdesc *) 0)
extern void prom_meminit(void);
#define PROM_NULL_COMPONENT ((pcomponent *) 0)
extern void prom_identify_arch(void);
extern PCHAR ArcGetEnvironmentVariable(PCHAR name);
extern void prom_init_cmdline(int argc, LONG *argv);
extern LONG ArcRead(ULONG fd, PVOID buf, ULONG num, PULONG cnt);
extern LONG ArcWrite(ULONG fd, PVOID buf, ULONG num, PULONG cnt);
extern VOID ArcEnterInteractiveMode(VOID) __noreturn;
extern DISPLAY_STATUS *ArcGetDisplayStatus(ULONG FileID);
#endif  
