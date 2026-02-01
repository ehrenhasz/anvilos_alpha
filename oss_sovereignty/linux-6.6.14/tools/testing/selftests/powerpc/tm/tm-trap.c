
 

#define _GNU_SOURCE
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <htmintrin.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>

#include "tm.h"
#include "utils.h"

#define pr_error(error_code, format, ...) \
	error_at_line(1, error_code, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define MSR_LE 1UL
#define LE     1UL

pthread_t t0_ping;
pthread_t t1_pong;

int exit_from_pong;

int trap_event;
int le;

bool success;

void trap_signal_handler(int signo, siginfo_t *si, void *uc)
{
	ucontext_t *ucp = uc;
	uint64_t thread_endianness;

	 
	thread_endianness = MSR_LE & ucp->uc_mcontext.gp_regs[PT_MSR];

	 

	if (le) {
		 
		if (trap_event == 0) {
			 
		}
		 
		else if (trap_event == 1) {
			 

			if (thread_endianness == LE) {
				 
				ucp->uc_mcontext.gp_regs[PT_NIP] += 16;
			} else {
				 
				ucp->uc_mcontext.gp_regs[PT_MSR] |= 1UL;
				ucp->uc_mcontext.gp_regs[PT_NIP] += 4;
			}
		}
	}

	 

	else {
		 
		if (trap_event == 0) {
			 
			ucp->uc_mcontext.gp_regs[PT_MSR] |= 1UL;
		}
		 
		else if (trap_event == 1) {
			 
		}
		 
		else {
			 

			 
			ucp->uc_mcontext.gp_regs[PT_MSR] &= ~1UL;
			ucp->uc_mcontext.gp_regs[PT_NIP] += 8;
		}
	}

	trap_event++;
}

void usr1_signal_handler(int signo, siginfo_t *si, void *not_used)
{
	 
	exit_from_pong = 1;
}

void *ping(void *not_used)
{
	uint64_t i;

	trap_event = 0;

	 
	for (i = 0; i < 1024*1024*512; i++)
		;

	asm goto(
		 

		" tbegin.        ;"  
		" tdi  0, 0, 0x48;"  
		" trap           ;"  
		".long 0x1D05007C;"  
		".long 0x0800E07F;"  
		" b %l[failure]  ;"  
		" b %l[success]  ;"  

		: : : : failure, success);

failure:
	success = false;
	goto exit_from_ping;

success:
	success = true;

exit_from_ping:
	 
	pthread_kill(t1_pong, SIGUSR1);
	return NULL;
}

void *pong(void *not_used)
{
	while (!exit_from_pong)
		 
		sched_yield();

	return NULL;
}

int tm_trap_test(void)
{
	uint16_t k = 1;
	int cpu, rc;

	pthread_attr_t attr;
	cpu_set_t cpuset;

	struct sigaction trap_sa;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	trap_sa.sa_flags = SA_SIGINFO;
	trap_sa.sa_sigaction = trap_signal_handler;
	sigaction(SIGTRAP, &trap_sa, NULL);

	struct sigaction usr1_sa;

	usr1_sa.sa_flags = SA_SIGINFO;
	usr1_sa.sa_sigaction = usr1_signal_handler;
	sigaction(SIGUSR1, &usr1_sa, NULL);

	cpu = pick_online_cpu();
	FAIL_IF(cpu < 0);

	
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);

	 
	rc = pthread_attr_init(&attr);
	if (rc)
		pr_error(rc, "pthread_attr_init()");

	 
	rc = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
	if (rc)
		pr_error(rc, "pthread_attr_setaffinity()");

	 
	le = (int) *(uint8_t *)&k;

	printf("%s machine detected. Checking if endianness flips %s",
		le ? "Little-Endian" : "Big-Endian",
		"inadvertently on trap in TM... ");

	rc = fflush(0);
	if (rc)
		pr_error(rc, "fflush()");

	 
	rc = pthread_create(&t0_ping, &attr, ping, NULL);
	if (rc)
		pr_error(rc, "pthread_create()");

	exit_from_pong = 0;

	 
	rc = pthread_create(&t1_pong, &attr, pong, NULL);
	if (rc)
		pr_error(rc, "pthread_create()");

	rc = pthread_join(t0_ping, NULL);
	if (rc)
		pr_error(rc, "pthread_join()");

	rc = pthread_join(t1_pong, NULL);
	if (rc)
		pr_error(rc, "pthread_join()");

	if (success) {
		printf("no.\n");  
		return EXIT_SUCCESS;
	}

	printf("yes!\n");  
	return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
	return test_harness(tm_trap_test, "tm_trap_test");
}
