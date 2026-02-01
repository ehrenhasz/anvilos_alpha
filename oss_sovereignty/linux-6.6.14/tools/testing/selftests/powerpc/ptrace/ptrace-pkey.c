
 
#include "ptrace.h"
#include "child.h"

#ifndef __NR_pkey_alloc
#define __NR_pkey_alloc		384
#endif

#ifndef __NR_pkey_free
#define __NR_pkey_free		385
#endif

#ifndef NT_PPC_PKEY
#define NT_PPC_PKEY		0x110
#endif

#ifndef PKEY_DISABLE_EXECUTE
#define PKEY_DISABLE_EXECUTE	0x4
#endif

#define AMR_BITS_PER_PKEY 2
#define PKEY_REG_BITS (sizeof(u64) * 8)
#define pkeyshift(pkey) (PKEY_REG_BITS - ((pkey + 1) * AMR_BITS_PER_PKEY))

static const char user_read[] = "[User Read (Running)]";
static const char user_write[] = "[User Write (Running)]";
static const char ptrace_read_running[] = "[Ptrace Read (Running)]";
static const char ptrace_write_running[] = "[Ptrace Write (Running)]";

 
struct shared_info {
	struct child_sync child_sync;

	 
	unsigned long amr1;

	 
	unsigned long amr2;

	 
	unsigned long invalid_amr;

	 
	unsigned long expected_iamr;

	 
	unsigned long expected_uamor;

	 
	unsigned long invalid_iamr;
	unsigned long invalid_uamor;
};

static int sys_pkey_alloc(unsigned long flags, unsigned long init_access_rights)
{
	return syscall(__NR_pkey_alloc, flags, init_access_rights);
}

static int child(struct shared_info *info)
{
	unsigned long reg;
	bool disable_execute = true;
	int pkey1, pkey2, pkey3;
	int ret;

	 
	ret = wait_parent(&info->child_sync);
	if (ret)
		return ret;

	 
	pkey1 = sys_pkey_alloc(0, PKEY_DISABLE_EXECUTE);
	if (pkey1 < 0) {
		pkey1 = sys_pkey_alloc(0, 0);
		CHILD_FAIL_IF(pkey1 < 0, &info->child_sync);

		disable_execute = false;
	}

	pkey2 = sys_pkey_alloc(0, 0);
	CHILD_FAIL_IF(pkey2 < 0, &info->child_sync);

	pkey3 = sys_pkey_alloc(0, 0);
	CHILD_FAIL_IF(pkey3 < 0, &info->child_sync);

	info->amr1 |= 3ul << pkeyshift(pkey1);
	info->amr2 |= 3ul << pkeyshift(pkey2);
	 
	info->invalid_amr = info->amr2 | (~0x0UL & ~info->expected_uamor);

	 
	if (disable_execute)
		info->expected_iamr |= 1ul << pkeyshift(pkey1);
	else
		info->expected_iamr &= ~(1ul << pkeyshift(pkey1));

	 
	info->expected_iamr &= ~(1ul << pkeyshift(pkey2));
	info->expected_iamr &= ~(1ul << pkeyshift(pkey3));

	 
	info->invalid_iamr = info->expected_iamr | (1ul << pkeyshift(pkey1) | 1ul << pkeyshift(pkey2));
	info->invalid_uamor = info->expected_uamor & ~(0x3ul << pkeyshift(pkey1));

	printf("%-30s AMR: %016lx pkey1: %d pkey2: %d pkey3: %d\n",
	       user_write, info->amr1, pkey1, pkey2, pkey3);

	set_amr(info->amr1);

	 
	ret = prod_parent(&info->child_sync);
	CHILD_FAIL_IF(ret, &info->child_sync);

	ret = wait_parent(&info->child_sync);
	if (ret)
		return ret;

	reg = mfspr(SPRN_AMR);

	printf("%-30s AMR: %016lx\n", user_read, reg);

	CHILD_FAIL_IF(reg != info->amr2, &info->child_sync);

	 
	ret = prod_parent(&info->child_sync);
	CHILD_FAIL_IF(ret, &info->child_sync);

	ret = wait_parent(&info->child_sync);
	if (ret)
		return ret;

	reg = mfspr(SPRN_AMR);

	printf("%-30s AMR: %016lx\n", user_read, reg);

	CHILD_FAIL_IF(reg != info->amr2, &info->child_sync);

	 
	ret = prod_parent(&info->child_sync);
	CHILD_FAIL_IF(ret, &info->child_sync);

	ret = wait_parent(&info->child_sync);
	if (ret)
		return ret;

	reg = mfspr(SPRN_AMR);

	printf("%-30s AMR: %016lx\n", user_read, reg);

	CHILD_FAIL_IF(reg != info->amr2, &info->child_sync);

	 

	ret = prod_parent(&info->child_sync);
	CHILD_FAIL_IF(ret, &info->child_sync);

	return TEST_PASS;
}

static int parent(struct shared_info *info, pid_t pid)
{
	unsigned long regs[3];
	int ret, status;

	 
	ret = ptrace_read_regs(pid, NT_PPC_PKEY, regs, 3);
	PARENT_SKIP_IF_UNSUPPORTED(ret, &info->child_sync, "PKEYs not supported");
	PARENT_FAIL_IF(ret, &info->child_sync);

	info->amr1 = info->amr2 = regs[0];
	info->expected_iamr = regs[1];
	info->expected_uamor = regs[2];

	 
	ret = prod_child(&info->child_sync);
	PARENT_FAIL_IF(ret, &info->child_sync);

	ret = wait_child(&info->child_sync);
	if (ret)
		return ret;

	 
	ret = ptrace_read_regs(pid, NT_PPC_PKEY, regs, 3);
	PARENT_FAIL_IF(ret, &info->child_sync);

	printf("%-30s AMR: %016lx IAMR: %016lx UAMOR: %016lx\n",
	       ptrace_read_running, regs[0], regs[1], regs[2]);

	PARENT_FAIL_IF(regs[0] != info->amr1, &info->child_sync);
	PARENT_FAIL_IF(regs[1] != info->expected_iamr, &info->child_sync);
	PARENT_FAIL_IF(regs[2] != info->expected_uamor, &info->child_sync);

	 
	ret = ptrace_write_regs(pid, NT_PPC_PKEY, &info->amr2, 1);
	PARENT_FAIL_IF(ret, &info->child_sync);

	printf("%-30s AMR: %016lx\n", ptrace_write_running, info->amr2);

	 
	ret = prod_child(&info->child_sync);
	PARENT_FAIL_IF(ret, &info->child_sync);

	ret = wait_child(&info->child_sync);
	if (ret)
		return ret;

	 
	ret = ptrace_write_regs(pid, NT_PPC_PKEY, &info->invalid_amr, 1);
	PARENT_FAIL_IF(ret, &info->child_sync);

	printf("%-30s AMR: %016lx\n", ptrace_write_running, info->invalid_amr);

	 
	ret = prod_child(&info->child_sync);
	PARENT_FAIL_IF(ret, &info->child_sync);

	ret = wait_child(&info->child_sync);
	if (ret)
		return ret;

	 
	regs[0] = info->amr1;
	regs[1] = info->invalid_iamr;
	ret = ptrace_write_regs(pid, NT_PPC_PKEY, regs, 2);
	PARENT_FAIL_IF(!ret, &info->child_sync);

	printf("%-30s AMR: %016lx IAMR: %016lx\n",
	       ptrace_write_running, regs[0], regs[1]);

	 
	regs[2] = info->invalid_uamor;
	ret = ptrace_write_regs(pid, NT_PPC_PKEY, regs, 3);
	PARENT_FAIL_IF(!ret, &info->child_sync);

	printf("%-30s AMR: %016lx IAMR: %016lx UAMOR: %016lx\n",
	       ptrace_write_running, regs[0], regs[1], regs[2]);

	 
	ret = ptrace_read_regs(pid, NT_PPC_PKEY, regs, 3);
	PARENT_FAIL_IF(ret, &info->child_sync);

	printf("%-30s AMR: %016lx IAMR: %016lx UAMOR: %016lx\n",
	       ptrace_read_running, regs[0], regs[1], regs[2]);

	PARENT_FAIL_IF(regs[0] != info->amr2, &info->child_sync);
	PARENT_FAIL_IF(regs[1] != info->expected_iamr, &info->child_sync);
	PARENT_FAIL_IF(regs[2] != info->expected_uamor, &info->child_sync);

	 
	ret = prod_child(&info->child_sync);
	PARENT_FAIL_IF(ret, &info->child_sync);

	ret = wait(&status);
	if (ret != pid) {
		printf("Child's exit status not captured\n");
		ret = TEST_PASS;
	} else if (!WIFEXITED(status)) {
		printf("Child exited abnormally\n");
		ret = TEST_FAIL;
	} else
		ret = WEXITSTATUS(status) ? TEST_FAIL : TEST_PASS;

	return ret;
}

static int ptrace_pkey(void)
{
	struct shared_info *info;
	int shm_id;
	int ret;
	pid_t pid;

	shm_id = shmget(IPC_PRIVATE, sizeof(*info), 0777 | IPC_CREAT);
	info = shmat(shm_id, NULL, 0);

	ret = init_child_sync(&info->child_sync);
	if (ret)
		return ret;

	pid = fork();
	if (pid < 0) {
		perror("fork() failed");
		ret = TEST_FAIL;
	} else if (pid == 0)
		ret = child(info);
	else
		ret = parent(info, pid);

	shmdt(info);

	if (pid) {
		destroy_child_sync(&info->child_sync);
		shmctl(shm_id, IPC_RMID, NULL);
	}

	return ret;
}

int main(int argc, char *argv[])
{
	return test_harness(ptrace_pkey, "ptrace_pkey");
}
