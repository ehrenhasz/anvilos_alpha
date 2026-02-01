

 

#include <asm-generic/errno-base.h>
#include <sys/stat.h>
#include <test_progs.h>
#include <linux/limits.h>

#include "local_storage.skel.h"
#include "network_helpers.h"
#include "task_local_storage_helpers.h"

#define TEST_STORAGE_VALUE 0xbeefdead

struct storage {
	void *inode;
	unsigned int value;
};

 
static int run_self_unlink(struct local_storage *skel, const char *rm_path)
{
	int child_pid, child_status, ret;
	int null_fd;

	child_pid = fork();
	if (child_pid == 0) {
		null_fd = open("/dev/null", O_WRONLY);
		dup2(null_fd, STDOUT_FILENO);
		dup2(null_fd, STDERR_FILENO);
		close(null_fd);

		skel->bss->monitored_pid = getpid();
		 
		ret = execlp(rm_path, rm_path, rm_path, NULL);
		if (ret)
			exit(errno);
	} else if (child_pid > 0) {
		waitpid(child_pid, &child_status, 0);
		ASSERT_EQ(skel->data->task_storage_result, 0, "task_storage_result");
		return WEXITSTATUS(child_status);
	}

	return -EINVAL;
}

static bool check_syscall_operations(int map_fd, int obj_fd)
{
	struct storage val = { .value = TEST_STORAGE_VALUE },
		       lookup_val = { .value = 0 };
	int err;

	 
	err = bpf_map_lookup_elem_flags(map_fd, &obj_fd, &lookup_val, 0);
	if (!ASSERT_EQ(err, -ENOENT, "bpf_map_lookup_elem"))
		return false;

	 
	err = bpf_map_update_elem(map_fd, &obj_fd, &val, BPF_NOEXIST);
	if (!ASSERT_OK(err, "bpf_map_update_elem"))
		return false;

	 
	err = bpf_map_lookup_elem_flags(map_fd, &obj_fd, &lookup_val, 0);
	if (!ASSERT_OK(err, "bpf_map_lookup_elem"))
		return false;

	 
	if (!ASSERT_EQ(lookup_val.value, val.value, "bpf_map_lookup_elem"))
		return false;

	err = bpf_map_delete_elem(map_fd, &obj_fd);
	if (!ASSERT_OK(err, "bpf_map_delete_elem()"))
		return false;

	 
	err = bpf_map_lookup_elem_flags(map_fd, &obj_fd, &lookup_val, 0);
	if (!ASSERT_EQ(err, -ENOENT, "bpf_map_lookup_elem"))
		return false;

	return true;
}

void test_test_local_storage(void)
{
	char tmp_dir_path[] = "/tmp/local_storageXXXXXX";
	int err, serv_sk = -1, task_fd = -1, rm_fd = -1;
	struct local_storage *skel = NULL;
	char tmp_exec_path[64];
	char cmd[256];

	skel = local_storage__open_and_load();
	if (!ASSERT_OK_PTR(skel, "skel_load"))
		goto close_prog;

	err = local_storage__attach(skel);
	if (!ASSERT_OK(err, "attach"))
		goto close_prog;

	task_fd = sys_pidfd_open(getpid(), 0);
	if (!ASSERT_GE(task_fd, 0, "pidfd_open"))
		goto close_prog;

	if (!check_syscall_operations(bpf_map__fd(skel->maps.task_storage_map),
				      task_fd))
		goto close_prog;

	if (!ASSERT_OK_PTR(mkdtemp(tmp_dir_path), "mkdtemp"))
		goto close_prog;

	snprintf(tmp_exec_path, sizeof(tmp_exec_path), "%s/copy_of_rm",
		 tmp_dir_path);
	snprintf(cmd, sizeof(cmd), "cp /bin/rm %s", tmp_exec_path);
	if (!ASSERT_OK(system(cmd), "system(cp)"))
		goto close_prog_rmdir;

	rm_fd = open(tmp_exec_path, O_RDONLY);
	if (!ASSERT_GE(rm_fd, 0, "open(tmp_exec_path)"))
		goto close_prog_rmdir;

	if (!check_syscall_operations(bpf_map__fd(skel->maps.inode_storage_map),
				      rm_fd))
		goto close_prog_rmdir;

	 
	err = run_self_unlink(skel, tmp_exec_path);
	if (!ASSERT_EQ(err, EPERM, "run_self_unlink"))
		goto close_prog_rmdir;

	 
	skel->bss->monitored_pid = getpid();

	 
	snprintf(cmd, sizeof(cmd), "mv %s/copy_of_rm %s/check_null_ptr",
		 tmp_dir_path, tmp_dir_path);
	if (!ASSERT_OK(system(cmd), "system(mv)"))
		goto close_prog_rmdir;

	ASSERT_EQ(skel->data->inode_storage_result, 0, "inode_storage_result");

	serv_sk = start_server(AF_INET6, SOCK_STREAM, NULL, 0, 0);
	if (!ASSERT_GE(serv_sk, 0, "start_server"))
		goto close_prog_rmdir;

	ASSERT_EQ(skel->data->sk_storage_result, 0, "sk_storage_result");

	if (!check_syscall_operations(bpf_map__fd(skel->maps.sk_storage_map),
				      serv_sk))
		goto close_prog_rmdir;

close_prog_rmdir:
	snprintf(cmd, sizeof(cmd), "rm -rf %s", tmp_dir_path);
	system(cmd);
close_prog:
	close(serv_sk);
	close(rm_fd);
	close(task_fd);
	local_storage__destroy(skel);
}
