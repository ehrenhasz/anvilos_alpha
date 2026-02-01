 
 

#define BUILD_VDSOX32

#include "vdso-layout.lds.S"

 
VERSION {
	LINUX_2.6 {
	global:
		__vdso_clock_gettime;
		__vdso_gettimeofday;
		__vdso_getcpu;
		__vdso_time;
		__vdso_clock_getres;
	local: *;
	};
}
