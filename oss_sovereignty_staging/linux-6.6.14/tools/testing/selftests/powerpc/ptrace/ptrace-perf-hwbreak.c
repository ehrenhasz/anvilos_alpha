

#include <asm/unistd.h>
#include <linux/hw_breakpoint.h>
#include <linux/ptrace.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "utils.h"

 
void same_watch_addr_child(unsigned long *addr);

 
extern char same_watch_addr_load[];

 
extern char same_watch_addr_trap[];

 
void perf_then_ptrace_child(unsigned long *first_addr, unsigned long *second_addr);

 
extern char perf_then_ptrace_load1[];

 
extern char perf_then_ptrace_load2[];

 
extern char perf_then_ptrace_trap[];

static inline long sys_ptrace(long request, pid_t pid, unsigned long addr, unsigned long data)
{
	return syscall(__NR_ptrace, request, pid, addr, data);
}

static long ptrace_traceme(void)
{
	return sys_ptrace(PTRACE_TRACEME, 0, 0, 0);
}

static long ptrace_getregs(pid_t pid, struct pt_regs *result)
{
	return sys_ptrace(PTRACE_GETREGS, pid, 0, (unsigned long)result);
}

static long ptrace_setregs(pid_t pid, struct pt_regs *result)
{
	return sys_ptrace(PTRACE_SETREGS, pid, 0, (unsigned long)result);
}

static long ptrace_cont(pid_t pid, long signal)
{
	return sys_ptrace(PTRACE_CONT, pid, 0, signal);
}

static long ptrace_singlestep(pid_t pid, long signal)
{
	return sys_ptrace(PTRACE_SINGLESTEP, pid, 0, signal);
}

static long ppc_ptrace_gethwdbginfo(pid_t pid, struct ppc_debug_info *dbginfo)
{
	return sys_ptrace(PPC_PTRACE_GETHWDBGINFO, pid, 0, (unsigned long)dbginfo);
}

static long ppc_ptrace_sethwdbg(pid_t pid, struct ppc_hw_breakpoint *bp_info)
{
	return sys_ptrace(PPC_PTRACE_SETHWDEBUG, pid, 0, (unsigned long)bp_info);
}

static long ppc_ptrace_delhwdbg(pid_t pid, int bp_id)
{
	return sys_ptrace(PPC_PTRACE_DELHWDEBUG, pid, 0L, bp_id);
}

static long ptrace_getreg_pc(pid_t pid, void **pc)
{
	struct pt_regs regs;
	long err;

	err = ptrace_getregs(pid, &regs);
	if (err)
		return err;

	*pc = (void *)regs.nip;

	return 0;
}

static long ptrace_setreg_pc(pid_t pid, void *pc)
{
	struct pt_regs regs;
	long err;

	err = ptrace_getregs(pid, &regs);
	if (err)
		return err;

	regs.nip = (unsigned long)pc;

	err = ptrace_setregs(pid, &regs);
	if (err)
		return err;

	return 0;
}

static int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu,
			   int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static void perf_user_event_attr_set(struct perf_event_attr *attr, void *addr, u64 len)
{
	memset(attr, 0, sizeof(struct perf_event_attr));

	attr->type		= PERF_TYPE_BREAKPOINT;
	attr->size		= sizeof(struct perf_event_attr);
	attr->bp_type		= HW_BREAKPOINT_R;
	attr->bp_addr		= (u64)addr;
	attr->bp_len		= len;
	attr->exclude_kernel	= 1;
	attr->exclude_hv	= 1;
}

static int perf_watchpoint_open(pid_t child_pid, void *addr, u64 len)
{
	struct perf_event_attr attr;

	perf_user_event_attr_set(&attr, addr, len);
	return perf_event_open(&attr, child_pid, -1, -1, 0);
}

static int perf_read_counter(int perf_fd, u64 *count)
{
	 
	ssize_t len = read(perf_fd, count, sizeof(*count));

	if (len != sizeof(*count))
		return -1;

	return 0;
}

static void ppc_ptrace_init_breakpoint(struct ppc_hw_breakpoint *info,
				       int type, void *addr, int len)
{
	info->version = 1;
	info->trigger_type = type;
	info->condition_mode = PPC_BREAKPOINT_CONDITION_NONE;
	info->addr = (u64)addr;
	info->addr2 = (u64)addr + len;
	info->condition_value = 0;
	if (!len)
		info->addr_mode = PPC_BREAKPOINT_MODE_EXACT;
	else
		info->addr_mode = PPC_BREAKPOINT_MODE_RANGE_INCLUSIVE;
}

 
static int check_watchpoints(pid_t pid)
{
	struct ppc_debug_info dbginfo;

	FAIL_IF_MSG(ppc_ptrace_gethwdbginfo(pid, &dbginfo), "PPC_PTRACE_GETHWDBGINFO failed");
	SKIP_IF_MSG(dbginfo.num_data_bps <= 1, "Not enough data watchpoints (need at least 2)");

	return 0;
}

 
static int ptrace_fork_child(pid_t *pid)
{
	int status;

	*pid = fork();

	if (*pid < 0)
		FAIL_IF_MSG(1, "Failed to fork child");

	if (!*pid) {
		FAIL_IF_EXIT_MSG(ptrace_traceme(), "PTRACE_TRACEME failed");
		FAIL_IF_EXIT_MSG(raise(SIGSTOP), "Child failed to raise SIGSTOP");
	} else {
		 
		FAIL_IF_MSG(waitpid(*pid, &status, 0) == -1, "Failed to wait for child");
		FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	}

	return 0;
}

 
int same_watch_addr_test(void)
{
	struct ppc_hw_breakpoint bp_info;	 
	int bp_id;	 
	int perf_fd;	 
	u64 perf_count;	 
	pid_t pid;	 
	void *pc;	 
	int status;	 
	unsigned long value;	 
	int err;

	err = ptrace_fork_child(&pid);
	if (err)
		return err;

	if (!pid) {
		same_watch_addr_child(&value);
		exit(1);
	}

	err = check_watchpoints(pid);
	if (err)
		return err;

	 
	perf_fd = perf_watchpoint_open(pid, &value, sizeof(value));
	FAIL_IF_MSG(perf_fd < 0, "Failed to open perf performance counter");

	 
	ppc_ptrace_init_breakpoint(&bp_info, PPC_BREAKPOINT_TRIGGER_READ, &value, sizeof(value));
	bp_id = ppc_ptrace_sethwdbg(pid, &bp_info);
	FAIL_IF_MSG(bp_id < 0, "Failed to set ptrace watchpoint");

	 
	FAIL_IF_MSG(ptrace_cont(pid, 0), "Failed to continue child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != same_watch_addr_load, "Child did not stop on load instruction");

	 
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 0, "perf recorded unexpected event");

	 
	FAIL_IF_MSG(ptrace_singlestep(pid, 0), "Failed to single step child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != same_watch_addr_load + 4, "Failed to single step load instruction");
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 1, "perf counter did not increment");

	 
	FAIL_IF_MSG(ppc_ptrace_delhwdbg(pid, bp_id), "Failed to remove old ptrace watchpoint");
	bp_id = ppc_ptrace_sethwdbg(pid, &bp_info);
	FAIL_IF_MSG(bp_id < 0, "Failed to set ptrace watchpoint");
	FAIL_IF_MSG(ptrace_setreg_pc(pid, same_watch_addr_load), "Failed to set child PC");
	FAIL_IF_MSG(ptrace_cont(pid, 0), "Failed to continue child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != same_watch_addr_load, "Child did not stop on load trap");
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 1, "perf counter should not have changed");

	 
	FAIL_IF_MSG(ptrace_cont(pid, 0), "Failed to continue child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != same_watch_addr_trap, "Child did not stop on end trap");
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 2, "perf counter did not increment");

	 
	FAIL_IF_MSG(ptrace_setreg_pc(pid, same_watch_addr_load), "Failed to set child PC");
	FAIL_IF_MSG(ptrace_cont(pid, 0), "Failed to continue child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != same_watch_addr_trap, "Child did not stop on end trap");
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 3, "perf counter did not increment");

	 
	FAIL_IF_MSG(ppc_ptrace_delhwdbg(pid, bp_id), "Failed to remove old ptrace watchpoint");
	bp_id = ppc_ptrace_sethwdbg(pid, &bp_info);
	FAIL_IF_MSG(bp_id < 0, "Failed to set ptrace watchpoint");
	FAIL_IF_MSG(ptrace_setreg_pc(pid, same_watch_addr_load), "Failed to set child PC");
	FAIL_IF_MSG(ptrace_cont(pid, 0), "Failed to continue child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != same_watch_addr_load, "Child did not stop on load instruction");
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 3, "perf counter should not have changed");

	 
	FAIL_IF_MSG(ptrace_setreg_pc(pid, same_watch_addr_load + 4), "Failed to set child PC");
	FAIL_IF_MSG(ptrace_cont(pid, 0), "Failed to continue child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != same_watch_addr_trap, "Child did not stop on end trap");
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 3, "perf counter should not have changed");

	 
	FAIL_IF_MSG(kill(pid, SIGKILL) != 0, "Failed to kill child");

	return 0;
}

 
int perf_then_ptrace_test(void)
{
	struct ppc_hw_breakpoint bp_info;	 
	int bp_id;	 
	int perf_fd;	 
	u64 perf_count;	 
	pid_t pid;	 
	void *pc;	 
	int status;	 
	unsigned long perf_value;	 
	unsigned long ptrace_value;	 
	int err;

	err = ptrace_fork_child(&pid);
	if (err)
		return err;

	 
	if (!pid) {
		perf_then_ptrace_child(&perf_value, &ptrace_value);
		exit(0);
	}

	err = check_watchpoints(pid);
	if (err)
		return err;

	 
	perf_fd = perf_watchpoint_open(pid, &perf_value, sizeof(perf_value));
	FAIL_IF_MSG(perf_fd < 0, "Failed to open perf performance counter");

	 
	ppc_ptrace_init_breakpoint(&bp_info, PPC_BREAKPOINT_TRIGGER_READ,
				   &ptrace_value, sizeof(ptrace_value));
	bp_id = ppc_ptrace_sethwdbg(pid, &bp_info);
	FAIL_IF_MSG(bp_id < 0, "Failed to set ptrace watchpoint");

	 
	FAIL_IF_MSG(ptrace_cont(pid, 0), "Failed to continue child");

	FAIL_IF_MSG(waitpid(pid, &status, 0) == -1, "Failed to wait for child");
	FAIL_IF_MSG(!WIFSTOPPED(status), "Child is not stopped");
	FAIL_IF_MSG(ptrace_getreg_pc(pid, &pc), "Failed to get child PC");
	FAIL_IF_MSG(pc != perf_then_ptrace_load2, "Child did not stop on ptrace load");

	 
	FAIL_IF_MSG(perf_read_counter(perf_fd, &perf_count), "Failed to read perf counter");
	FAIL_IF_MSG(perf_count != 1, "perf counter did not increment");

	 
	FAIL_IF_MSG(kill(pid, SIGKILL) != 0, "Failed to kill child");

	return 0;
}

int main(int argc, char *argv[])
{
	int err = 0;

	err |= test_harness(same_watch_addr_test, "same_watch_addr");
	err |= test_harness(perf_then_ptrace_test, "perf_then_ptrace");

	return err;
}
