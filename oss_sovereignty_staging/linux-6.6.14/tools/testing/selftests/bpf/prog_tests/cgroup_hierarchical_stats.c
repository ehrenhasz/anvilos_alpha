
 
#include <asm-generic/errno.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <test_progs.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "cgroup_helpers.h"
#include "cgroup_hierarchical_stats.skel.h"

#define PAGE_SIZE 4096
#define MB(x) (x << 20)

#define PROCESSES_PER_CGROUP 3

#define BPFFS_ROOT "/sys/fs/bpf/"
#define BPFFS_ATTACH_COUNTERS BPFFS_ROOT "attach_counters/"

#define CG_ROOT_NAME "root"
#define CG_ROOT_ID 1

#define CGROUP_PATH(p, n) {.path = p"/"n, .name = n}

static struct {
	const char *path, *name;
	unsigned long long id;
	int fd;
} cgroups[] = {
	CGROUP_PATH("/", "test"),
	CGROUP_PATH("/test", "child1"),
	CGROUP_PATH("/test", "child2"),
	CGROUP_PATH("/test/child1", "child1_1"),
	CGROUP_PATH("/test/child1", "child1_2"),
	CGROUP_PATH("/test/child2", "child2_1"),
	CGROUP_PATH("/test/child2", "child2_2"),
};

#define N_CGROUPS ARRAY_SIZE(cgroups)
#define N_NON_LEAF_CGROUPS 3

static int root_cgroup_fd;
static bool mounted_bpffs;

 
static int read_from_file(const char *path, char *buf, size_t size)
{
	int fd, len;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return fd;

	len = read(fd, buf, size);
	close(fd);
	if (len < 0)
		return len;

	buf[len] = 0;
	return 0;
}

 
static int setup_bpffs(void)
{
	int err;

	 
	err = mount("bpf", BPFFS_ROOT, "bpf", 0, NULL);
	mounted_bpffs = !err;
	if (ASSERT_FALSE(err && errno != EBUSY, "mount"))
		return err;

	 
	err = mkdir(BPFFS_ATTACH_COUNTERS, 0755);
	if (!ASSERT_OK(err, "mkdir"))
		return err;

	return 0;
}

static void cleanup_bpffs(void)
{
	 
	ASSERT_OK(rmdir(BPFFS_ATTACH_COUNTERS), "rmdir "BPFFS_ATTACH_COUNTERS);

	 
	if (mounted_bpffs)
		return;

	ASSERT_OK(umount(BPFFS_ROOT), "unmount bpffs");
}

 
static int setup_cgroups(void)
{
	int i, fd, err;

	err = setup_cgroup_environment();
	if (!ASSERT_OK(err, "setup_cgroup_environment"))
		return err;

	root_cgroup_fd = get_root_cgroup();
	if (!ASSERT_GE(root_cgroup_fd, 0, "get_root_cgroup"))
		return root_cgroup_fd;

	for (i = 0; i < N_CGROUPS; i++) {
		fd = create_and_get_cgroup(cgroups[i].path);
		if (!ASSERT_GE(fd, 0, "create_and_get_cgroup"))
			return fd;

		cgroups[i].fd = fd;
		cgroups[i].id = get_cgroup_id(cgroups[i].path);
	}
	return 0;
}

static void cleanup_cgroups(void)
{
	close(root_cgroup_fd);
	for (int i = 0; i < N_CGROUPS; i++)
		close(cgroups[i].fd);
	cleanup_cgroup_environment();
}

 
static int setup_hierarchy(void)
{
	return setup_bpffs() || setup_cgroups();
}

static void destroy_hierarchy(void)
{
	cleanup_cgroups();
	cleanup_bpffs();
}

static int attach_processes(void)
{
	int i, j, status;

	 
	for (i = N_NON_LEAF_CGROUPS; i < N_CGROUPS; i++) {
		for (j = 0; j < PROCESSES_PER_CGROUP; j++) {
			pid_t pid;

			 
			pid = fork();
			if (pid == 0) {
				if (join_parent_cgroup(cgroups[i].path))
					exit(EACCES);
				exit(0);
			}

			 
			waitpid(pid, &status, 0);
			if (!ASSERT_TRUE(WIFEXITED(status), "child process exited"))
				return 1;
			if (!ASSERT_EQ(WEXITSTATUS(status), 0,
				       "child process exit code"))
				return 1;
		}
	}
	return 0;
}

static unsigned long long
get_attach_counter(unsigned long long cgroup_id, const char *file_name)
{
	unsigned long long attach_counter = 0, id = 0;
	static char buf[128], path[128];

	 
	snprintf(path, 128, "%s%s", BPFFS_ATTACH_COUNTERS, file_name);
	if (!ASSERT_OK(read_from_file(path, buf, 128), "read cgroup_iter"))
		return 0;

	 
	ASSERT_EQ(sscanf(buf, "cg_id: %llu, attach_counter: %llu\n",
			 &id, &attach_counter), 2, "output format");

	 
	ASSERT_EQ(id, cgroup_id, "cgroup_id");
	 
	ASSERT_GT(attach_counter, 0, "attach counter non-zero");
	return attach_counter;
}

static void check_attach_counters(void)
{
	unsigned long long attach_counters[N_CGROUPS], root_attach_counter;
	int i;

	for (i = 0; i < N_CGROUPS; i++)
		attach_counters[i] = get_attach_counter(cgroups[i].id,
							cgroups[i].name);

	 
	root_attach_counter = get_attach_counter(CG_ROOT_ID, CG_ROOT_NAME);

	 
	for (i = N_NON_LEAF_CGROUPS; i < N_CGROUPS; i++)
		ASSERT_EQ(attach_counters[i], PROCESSES_PER_CGROUP,
			  "leaf cgroup attach counter");

	 
	ASSERT_EQ(attach_counters[1], attach_counters[3] + attach_counters[4],
		  "child1_counter");
	 
	ASSERT_EQ(attach_counters[2], attach_counters[5] + attach_counters[6],
		  "child2_counter");
	 
	ASSERT_EQ(attach_counters[0], attach_counters[1] + attach_counters[2],
		  "test_counter");
	 
	ASSERT_GE(root_attach_counter, attach_counters[1], "root_counter");
}

 
static int setup_cgroup_iter(struct cgroup_hierarchical_stats *obj,
			     int cgroup_fd, const char *file_name)
{
	DECLARE_LIBBPF_OPTS(bpf_iter_attach_opts, opts);
	union bpf_iter_link_info linfo = {};
	struct bpf_link *link;
	static char path[128];
	int err;

	 
	linfo.cgroup.cgroup_fd = cgroup_fd;
	linfo.cgroup.order = BPF_CGROUP_ITER_SELF_ONLY;
	opts.link_info = &linfo;
	opts.link_info_len = sizeof(linfo);
	link = bpf_program__attach_iter(obj->progs.dumper, &opts);
	if (!ASSERT_OK_PTR(link, "attach_iter"))
		return -EFAULT;

	 
	snprintf(path, 128, "%s%s", BPFFS_ATTACH_COUNTERS, file_name);
	err = bpf_link__pin(link, path);
	ASSERT_OK(err, "pin cgroup_iter");

	 
	bpf_link__destroy(link);
	return err;
}

 
static int setup_progs(struct cgroup_hierarchical_stats **skel)
{
	int i, err;

	*skel = cgroup_hierarchical_stats__open_and_load();
	if (!ASSERT_OK_PTR(*skel, "open_and_load"))
		return 1;

	 
	for (i = 0; i < N_CGROUPS; i++) {
		err = setup_cgroup_iter(*skel, cgroups[i].fd, cgroups[i].name);
		if (!ASSERT_OK(err, "setup_cgroup_iter"))
			return err;
	}

	 
	err = setup_cgroup_iter(*skel, root_cgroup_fd, CG_ROOT_NAME);
	if (!ASSERT_OK(err, "setup_cgroup_iter"))
		return err;

	bpf_program__set_autoattach((*skel)->progs.dumper, false);
	err = cgroup_hierarchical_stats__attach(*skel);
	if (!ASSERT_OK(err, "attach"))
		return err;

	return 0;
}

static void destroy_progs(struct cgroup_hierarchical_stats *skel)
{
	static char path[128];
	int i;

	for (i = 0; i < N_CGROUPS; i++) {
		 
		snprintf(path, 128, "%s%s", BPFFS_ATTACH_COUNTERS,
			 cgroups[i].name);
		ASSERT_OK(remove(path), "remove cgroup_iter pin");
	}

	 
	snprintf(path, 128, "%s%s", BPFFS_ATTACH_COUNTERS, CG_ROOT_NAME);
	ASSERT_OK(remove(path), "remove cgroup_iter root pin");
	cgroup_hierarchical_stats__destroy(skel);
}

void test_cgroup_hierarchical_stats(void)
{
	struct cgroup_hierarchical_stats *skel = NULL;

	if (setup_hierarchy())
		goto hierarchy_cleanup;
	if (setup_progs(&skel))
		goto cleanup;
	if (attach_processes())
		goto cleanup;
	check_attach_counters();
cleanup:
	destroy_progs(skel);
hierarchy_cleanup:
	destroy_hierarchy();
}
