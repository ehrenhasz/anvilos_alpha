


#include <asm/page.h>

#define BUILD_VDSO32

#include "../vdso-layout.lds.S"


ENTRY(__kernel_vsyscall);


VERSION
{
	LINUX_2.6 {
	global:
		__vdso_clock_gettime;
		__vdso_gettimeofday;
		__vdso_time;
		__vdso_clock_getres;
		__vdso_clock_gettime64;
		__vdso_getcpu;
	};

	LINUX_2.5 {
	global:
		__kernel_vsyscall;
		__kernel_sigreturn;
		__kernel_rt_sigreturn;
	local: *;
	};
}
