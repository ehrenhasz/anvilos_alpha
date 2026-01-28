


#define BUILD_VDSO64

#include "vdso-layout.lds.S"


VERSION {
	LINUX_2.6 {
	global:
		clock_gettime;
		__vdso_clock_gettime;
		gettimeofday;
		__vdso_gettimeofday;
		getcpu;
		__vdso_getcpu;
		time;
		__vdso_time;
		clock_getres;
		__vdso_clock_getres;
#ifdef CONFIG_X86_SGX
		__vdso_sgx_enter_enclave;
#endif
	local: *;
	};
}
