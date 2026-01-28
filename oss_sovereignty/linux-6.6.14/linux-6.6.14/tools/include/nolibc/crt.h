#ifndef _NOLIBC_CRT_H
#define _NOLIBC_CRT_H
char **environ __attribute__((weak));
const unsigned long *_auxv __attribute__((weak));
static void __stack_chk_init(void);
static void exit(int);
__attribute__((weak))
void _start_c(long *sp)
{
	long argc;
	char **argv;
	char **envp;
	const unsigned long *auxv;
	int _nolibc_main(int, char **, char **) __asm__ ("main");
	__stack_chk_init();
	argc = *sp;
	argv = (void *)(sp + 1);
	environ = envp = argv + argc + 1;
	for (auxv = (void *)envp; *auxv++;)
		;
	_auxv = auxv;
	exit(_nolibc_main(argc, argv, envp));
}
#endif  
