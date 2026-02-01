
#undef _GNU_SOURCE
#define _GNU_SOURCE 1
#undef __USE_GNU
#define __USE_GNU 1
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fenv.h>

enum {
	CF = 1 << 0,
	PF = 1 << 2,
	ZF = 1 << 6,
	ARITH = CF | PF | ZF,
};

long res_fcomi_pi_1;
long res_fcomi_1_pi;
long res_fcomi_1_1;
long res_fcomi_nan_1;
 
 
int snan = 0x7fc11111;
int qnan = 0x7f811111;
unsigned short snan1[5];
 
unsigned short snan80[5] = { 0x1111, 0x1111, 0x1111, 0x8111, 0x7fff };

int test(long flags)
{
	feclearexcept(FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);

	asm ("\n"

	"	push	%0""\n"
	"	popf""\n"
	"	fld1""\n"
	"	fldpi""\n"
	"	fcomi	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	ffree	%%st(1)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_1_pi""\n"

	"	push	%0""\n"
	"	popf""\n"
	"	fldpi""\n"
	"	fld1""\n"
	"	fcomi	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	ffree	%%st(1)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_pi_1""\n"

	"	push	%0""\n"
	"	popf""\n"
	"	fld1""\n"
	"	fld1""\n"
	"	fcomi	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	ffree	%%st(1)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_1_1""\n"
	:
	: "r" (flags)
	);
	if ((res_fcomi_1_pi & ARITH) != (0)) {
		printf("[BAD]\tfcomi_1_pi with flags:%lx\n", flags);
		return 1;
	}
	if ((res_fcomi_pi_1 & ARITH) != (CF)) {
		printf("[BAD]\tfcomi_pi_1 with flags:%lx->%lx\n", flags, res_fcomi_pi_1 & ARITH);
		return 1;
	}
	if ((res_fcomi_1_1 & ARITH) != (ZF)) {
		printf("[BAD]\tfcomi_1_1 with flags:%lx\n", flags);
		return 1;
	}
	if (fetestexcept(FE_INVALID) != 0) {
		printf("[BAD]\tFE_INVALID is set in %s\n", __func__);
		return 1;
	}
	return 0;
}

int test_qnan(long flags)
{
	feclearexcept(FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);

	asm ("\n"
	"	push	%0""\n"
	"	popf""\n"
	"	flds	qnan""\n"
	"	fld1""\n"
	"	fnclex""\n"		
	"	fcomi	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	ffree	%%st(1)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_nan_1""\n"
	:
	: "r" (flags)
	);
	if ((res_fcomi_nan_1 & ARITH) != (ZF|CF|PF)) {
		printf("[BAD]\tfcomi_qnan_1 with flags:%lx\n", flags);
		return 1;
	}
	if (fetestexcept(FE_INVALID) != FE_INVALID) {
		printf("[BAD]\tFE_INVALID is not set in %s\n", __func__);
		return 1;
	}
	return 0;
}

int testu_qnan(long flags)
{
	feclearexcept(FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);

	asm ("\n"
	"	push	%0""\n"
	"	popf""\n"
	"	flds	qnan""\n"
	"	fld1""\n"
	"	fnclex""\n"		
	"	fucomi	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	ffree	%%st(1)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_nan_1""\n"
	:
	: "r" (flags)
	);
	if ((res_fcomi_nan_1 & ARITH) != (ZF|CF|PF)) {
		printf("[BAD]\tfcomi_qnan_1 with flags:%lx\n", flags);
		return 1;
	}
	if (fetestexcept(FE_INVALID) != 0) {
		printf("[BAD]\tFE_INVALID is set in %s\n", __func__);
		return 1;
	}
	return 0;
}

int testu_snan(long flags)
{
	feclearexcept(FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);

	asm ("\n"
	"	push	%0""\n"
	"	popf""\n"



	"	fldt	snan80""\n"	
	"	fld1""\n"
	"	fucomi	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	ffree	%%st(1)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_nan_1""\n"
	:
	: "r" (flags)
	);
	if ((res_fcomi_nan_1 & ARITH) != (ZF|CF|PF)) {
		printf("[BAD]\tfcomi_qnan_1 with flags:%lx\n", flags);
		return 1;
	}

	if (fetestexcept(FE_INVALID) != FE_INVALID) {
		printf("[BAD]\tFE_INVALID is not set in %s\n", __func__);
		return 1;
	}
	return 0;
}

int testp(long flags)
{
	feclearexcept(FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);

	asm ("\n"

	"	push	%0""\n"
	"	popf""\n"
	"	fld1""\n"
	"	fldpi""\n"
	"	fcomip	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_1_pi""\n"

	"	push	%0""\n"
	"	popf""\n"
	"	fldpi""\n"
	"	fld1""\n"
	"	fcomip	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_pi_1""\n"

	"	push	%0""\n"
	"	popf""\n"
	"	fld1""\n"
	"	fld1""\n"
	"	fcomip	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_1_1""\n"
	:
	: "r" (flags)
	);
	if ((res_fcomi_1_pi & ARITH) != (0)) {
		printf("[BAD]\tfcomi_1_pi with flags:%lx\n", flags);
		return 1;
	}
	if ((res_fcomi_pi_1 & ARITH) != (CF)) {
		printf("[BAD]\tfcomi_pi_1 with flags:%lx->%lx\n", flags, res_fcomi_pi_1 & ARITH);
		return 1;
	}
	if ((res_fcomi_1_1 & ARITH) != (ZF)) {
		printf("[BAD]\tfcomi_1_1 with flags:%lx\n", flags);
		return 1;
	}
	if (fetestexcept(FE_INVALID) != 0) {
		printf("[BAD]\tFE_INVALID is set in %s\n", __func__);
		return 1;
	}
	return 0;
}

int testp_qnan(long flags)
{
	feclearexcept(FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);

	asm ("\n"
	"	push	%0""\n"
	"	popf""\n"
	"	flds	qnan""\n"
	"	fld1""\n"
	"	fnclex""\n"		
	"	fcomip	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_nan_1""\n"
	:
	: "r" (flags)
	);
	if ((res_fcomi_nan_1 & ARITH) != (ZF|CF|PF)) {
		printf("[BAD]\tfcomi_qnan_1 with flags:%lx\n", flags);
		return 1;
	}
	if (fetestexcept(FE_INVALID) != FE_INVALID) {
		printf("[BAD]\tFE_INVALID is not set in %s\n", __func__);
		return 1;
	}
	return 0;
}

int testup_qnan(long flags)
{
	feclearexcept(FE_DIVBYZERO|FE_INEXACT|FE_INVALID|FE_OVERFLOW|FE_UNDERFLOW);

	asm ("\n"
	"	push	%0""\n"
	"	popf""\n"
	"	flds	qnan""\n"
	"	fld1""\n"
	"	fnclex""\n"		
	"	fucomip	%%st(1), %%st" "\n"
	"	ffree	%%st(0)" "\n"
	"	pushf""\n"
	"	pop	res_fcomi_nan_1""\n"
	:
	: "r" (flags)
	);
	if ((res_fcomi_nan_1 & ARITH) != (ZF|CF|PF)) {
		printf("[BAD]\tfcomi_qnan_1 with flags:%lx\n", flags);
		return 1;
	}
	if (fetestexcept(FE_INVALID) != 0) {
		printf("[BAD]\tFE_INVALID is set in %s\n", __func__);
		return 1;
	}
	return 0;
}

void sighandler(int sig)
{
	printf("[FAIL]\tGot signal %d, exiting\n", sig);
	exit(1);
}

int main(int argc, char **argv, char **envp)
{
	int err = 0;

	 
	signal(SIGILL, sighandler);
	signal(SIGFPE, sighandler);
	signal(SIGSEGV, sighandler);

	printf("[RUN]\tTesting f[u]comi[p] instructions\n");
	err |= test(0);
	err |= test_qnan(0);
	err |= testu_qnan(0);
	err |= testu_snan(0);
	err |= test(CF|ZF|PF);
	err |= test_qnan(CF|ZF|PF);
	err |= testu_qnan(CF|ZF|PF);
	err |= testu_snan(CF|ZF|PF);
	err |= testp(0);
	err |= testp_qnan(0);
	err |= testup_qnan(0);
	err |= testp(CF|ZF|PF);
	err |= testp_qnan(CF|ZF|PF);
	err |= testup_qnan(CF|ZF|PF);
	if (!err)
		printf("[OK]\tf[u]comi[p]\n");
	else
		printf("[FAIL]\tf[u]comi[p] errors: %d\n", err);

	return err;
}
