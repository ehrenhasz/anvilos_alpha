
#include <asm/bug.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/zalloc.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#ifdef HAVE_LIBBPF_SUPPORT
#include <bpf/libbpf.h>
#include "bpf-event.h"
#include "bpf-utils.h"
#endif
#include "compress.h"
#include "env.h"
#include "namespaces.h"
#include "path.h"
#include "map.h"
#include "symbol.h"
#include "srcline.h"
#include "dso.h"
#include "dsos.h"
#include "machine.h"
#include "auxtrace.h"
#include "util.h"  
#include "debug.h"
#include "string2.h"
#include "vdso.h"

static const char * const debuglink_paths[] = {
	"%.0s%s",
	"%s/%s",
	"%s/.debug/%s",
	"/usr/lib/debug%s/%s"
};

char dso__symtab_origin(const struct dso *dso)
{
	static const char origin[] = {
		[DSO_BINARY_TYPE__KALLSYMS]			= 'k',
		[DSO_BINARY_TYPE__VMLINUX]			= 'v',
		[DSO_BINARY_TYPE__JAVA_JIT]			= 'j',
		[DSO_BINARY_TYPE__DEBUGLINK]			= 'l',
		[DSO_BINARY_TYPE__BUILD_ID_CACHE]		= 'B',
		[DSO_BINARY_TYPE__BUILD_ID_CACHE_DEBUGINFO]	= 'D',
		[DSO_BINARY_TYPE__FEDORA_DEBUGINFO]		= 'f',
		[DSO_BINARY_TYPE__UBUNTU_DEBUGINFO]		= 'u',
		[DSO_BINARY_TYPE__MIXEDUP_UBUNTU_DEBUGINFO]	= 'x',
		[DSO_BINARY_TYPE__OPENEMBEDDED_DEBUGINFO]	= 'o',
		[DSO_BINARY_TYPE__BUILDID_DEBUGINFO]		= 'b',
		[DSO_BINARY_TYPE__SYSTEM_PATH_DSO]		= 'd',
		[DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE]		= 'K',
		[DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE_COMP]	= 'm',
		[DSO_BINARY_TYPE__GUEST_KALLSYMS]		= 'g',
		[DSO_BINARY_TYPE__GUEST_KMODULE]		= 'G',
		[DSO_BINARY_TYPE__GUEST_KMODULE_COMP]		= 'M',
		[DSO_BINARY_TYPE__GUEST_VMLINUX]		= 'V',
	};

	if (dso == NULL || dso->symtab_type == DSO_BINARY_TYPE__NOT_FOUND)
		return '!';
	return origin[dso->symtab_type];
}

bool dso__is_object_file(const struct dso *dso)
{
	switch (dso->binary_type) {
	case DSO_BINARY_TYPE__KALLSYMS:
	case DSO_BINARY_TYPE__GUEST_KALLSYMS:
	case DSO_BINARY_TYPE__JAVA_JIT:
	case DSO_BINARY_TYPE__BPF_PROG_INFO:
	case DSO_BINARY_TYPE__BPF_IMAGE:
	case DSO_BINARY_TYPE__OOL:
		return false;
	case DSO_BINARY_TYPE__VMLINUX:
	case DSO_BINARY_TYPE__GUEST_VMLINUX:
	case DSO_BINARY_TYPE__DEBUGLINK:
	case DSO_BINARY_TYPE__BUILD_ID_CACHE:
	case DSO_BINARY_TYPE__BUILD_ID_CACHE_DEBUGINFO:
	case DSO_BINARY_TYPE__FEDORA_DEBUGINFO:
	case DSO_BINARY_TYPE__UBUNTU_DEBUGINFO:
	case DSO_BINARY_TYPE__MIXEDUP_UBUNTU_DEBUGINFO:
	case DSO_BINARY_TYPE__BUILDID_DEBUGINFO:
	case DSO_BINARY_TYPE__SYSTEM_PATH_DSO:
	case DSO_BINARY_TYPE__GUEST_KMODULE:
	case DSO_BINARY_TYPE__GUEST_KMODULE_COMP:
	case DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE:
	case DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE_COMP:
	case DSO_BINARY_TYPE__KCORE:
	case DSO_BINARY_TYPE__GUEST_KCORE:
	case DSO_BINARY_TYPE__OPENEMBEDDED_DEBUGINFO:
	case DSO_BINARY_TYPE__NOT_FOUND:
	default:
		return true;
	}
}

int dso__read_binary_type_filename(const struct dso *dso,
				   enum dso_binary_type type,
				   char *root_dir, char *filename, size_t size)
{
	char build_id_hex[SBUILD_ID_SIZE];
	int ret = 0;
	size_t len;

	switch (type) {
	case DSO_BINARY_TYPE__DEBUGLINK:
	{
		const char *last_slash;
		char dso_dir[PATH_MAX];
		char symfile[PATH_MAX];
		unsigned int i;

		len = __symbol__join_symfs(filename, size, dso->long_name);
		last_slash = filename + len;
		while (last_slash != filename && *last_slash != '/')
			last_slash--;

		strncpy(dso_dir, filename, last_slash - filename);
		dso_dir[last_slash-filename] = '\0';

		if (!is_regular_file(filename)) {
			ret = -1;
			break;
		}

		ret = filename__read_debuglink(filename, symfile, PATH_MAX);
		if (ret)
			break;

		 
		ret = -1;
		for (i = 0; i < ARRAY_SIZE(debuglink_paths); i++) {
			snprintf(filename, size,
					debuglink_paths[i], dso_dir, symfile);
			if (is_regular_file(filename)) {
				ret = 0;
				break;
			}
		}

		break;
	}
	case DSO_BINARY_TYPE__BUILD_ID_CACHE:
		if (dso__build_id_filename(dso, filename, size, false) == NULL)
			ret = -1;
		break;

	case DSO_BINARY_TYPE__BUILD_ID_CACHE_DEBUGINFO:
		if (dso__build_id_filename(dso, filename, size, true) == NULL)
			ret = -1;
		break;

	case DSO_BINARY_TYPE__FEDORA_DEBUGINFO:
		len = __symbol__join_symfs(filename, size, "/usr/lib/debug");
		snprintf(filename + len, size - len, "%s.debug", dso->long_name);
		break;

	case DSO_BINARY_TYPE__UBUNTU_DEBUGINFO:
		len = __symbol__join_symfs(filename, size, "/usr/lib/debug");
		snprintf(filename + len, size - len, "%s", dso->long_name);
		break;

	case DSO_BINARY_TYPE__MIXEDUP_UBUNTU_DEBUGINFO:
		 
		if (strlen(dso->long_name) < 9 ||
		    strncmp(dso->long_name, "/usr/lib/", 9)) {
			ret = -1;
			break;
		}
		len = __symbol__join_symfs(filename, size, "/usr/lib/debug");
		snprintf(filename + len, size - len, "%s", dso->long_name + 4);
		break;

	case DSO_BINARY_TYPE__OPENEMBEDDED_DEBUGINFO:
	{
		const char *last_slash;
		size_t dir_size;

		last_slash = dso->long_name + dso->long_name_len;
		while (last_slash != dso->long_name && *last_slash != '/')
			last_slash--;

		len = __symbol__join_symfs(filename, size, "");
		dir_size = last_slash - dso->long_name + 2;
		if (dir_size > (size - len)) {
			ret = -1;
			break;
		}
		len += scnprintf(filename + len, dir_size, "%s",  dso->long_name);
		len += scnprintf(filename + len , size - len, ".debug%s",
								last_slash);
		break;
	}

	case DSO_BINARY_TYPE__BUILDID_DEBUGINFO:
		if (!dso->has_build_id) {
			ret = -1;
			break;
		}

		build_id__sprintf(&dso->bid, build_id_hex);
		len = __symbol__join_symfs(filename, size, "/usr/lib/debug/.build-id/");
		snprintf(filename + len, size - len, "%.2s/%s.debug",
			 build_id_hex, build_id_hex + 2);
		break;

	case DSO_BINARY_TYPE__VMLINUX:
	case DSO_BINARY_TYPE__GUEST_VMLINUX:
	case DSO_BINARY_TYPE__SYSTEM_PATH_DSO:
		__symbol__join_symfs(filename, size, dso->long_name);
		break;

	case DSO_BINARY_TYPE__GUEST_KMODULE:
	case DSO_BINARY_TYPE__GUEST_KMODULE_COMP:
		path__join3(filename, size, symbol_conf.symfs,
			    root_dir, dso->long_name);
		break;

	case DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE:
	case DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE_COMP:
		__symbol__join_symfs(filename, size, dso->long_name);
		break;

	case DSO_BINARY_TYPE__KCORE:
	case DSO_BINARY_TYPE__GUEST_KCORE:
		snprintf(filename, size, "%s", dso->long_name);
		break;

	default:
	case DSO_BINARY_TYPE__KALLSYMS:
	case DSO_BINARY_TYPE__GUEST_KALLSYMS:
	case DSO_BINARY_TYPE__JAVA_JIT:
	case DSO_BINARY_TYPE__BPF_PROG_INFO:
	case DSO_BINARY_TYPE__BPF_IMAGE:
	case DSO_BINARY_TYPE__OOL:
	case DSO_BINARY_TYPE__NOT_FOUND:
		ret = -1;
		break;
	}

	return ret;
}

enum {
	COMP_ID__NONE = 0,
};

static const struct {
	const char *fmt;
	int (*decompress)(const char *input, int output);
	bool (*is_compressed)(const char *input);
} compressions[] = {
	[COMP_ID__NONE] = { .fmt = NULL, },
#ifdef HAVE_ZLIB_SUPPORT
	{ "gz", gzip_decompress_to_file, gzip_is_compressed },
#endif
#ifdef HAVE_LZMA_SUPPORT
	{ "xz", lzma_decompress_to_file, lzma_is_compressed },
#endif
	{ NULL, NULL, NULL },
};

static int is_supported_compression(const char *ext)
{
	unsigned i;

	for (i = 1; compressions[i].fmt; i++) {
		if (!strcmp(ext, compressions[i].fmt))
			return i;
	}
	return COMP_ID__NONE;
}

bool is_kernel_module(const char *pathname, int cpumode)
{
	struct kmod_path m;
	int mode = cpumode & PERF_RECORD_MISC_CPUMODE_MASK;

	WARN_ONCE(mode != cpumode,
		  "Internal error: passing unmasked cpumode (%x) to is_kernel_module",
		  cpumode);

	switch (mode) {
	case PERF_RECORD_MISC_USER:
	case PERF_RECORD_MISC_HYPERVISOR:
	case PERF_RECORD_MISC_GUEST_USER:
		return false;
	 
	default:
		if (kmod_path__parse(&m, pathname)) {
			pr_err("Failed to check whether %s is a kernel module or not. Assume it is.",
					pathname);
			return true;
		}
	}

	return m.kmod;
}

bool dso__needs_decompress(struct dso *dso)
{
	return dso->symtab_type == DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE_COMP ||
		dso->symtab_type == DSO_BINARY_TYPE__GUEST_KMODULE_COMP;
}

int filename__decompress(const char *name, char *pathname,
			 size_t len, int comp, int *err)
{
	char tmpbuf[] = KMOD_DECOMP_NAME;
	int fd = -1;

	 
	if (!compressions[comp].is_compressed(name))
		return open(name, O_RDONLY);

	fd = mkstemp(tmpbuf);
	if (fd < 0) {
		*err = errno;
		return -1;
	}

	if (compressions[comp].decompress(name, fd)) {
		*err = DSO_LOAD_ERRNO__DECOMPRESSION_FAILURE;
		close(fd);
		fd = -1;
	}

	if (!pathname || (fd < 0))
		unlink(tmpbuf);

	if (pathname && (fd >= 0))
		strlcpy(pathname, tmpbuf, len);

	return fd;
}

static int decompress_kmodule(struct dso *dso, const char *name,
			      char *pathname, size_t len)
{
	if (!dso__needs_decompress(dso))
		return -1;

	if (dso->comp == COMP_ID__NONE)
		return -1;

	return filename__decompress(name, pathname, len, dso->comp,
				    &dso->load_errno);
}

int dso__decompress_kmodule_fd(struct dso *dso, const char *name)
{
	return decompress_kmodule(dso, name, NULL, 0);
}

int dso__decompress_kmodule_path(struct dso *dso, const char *name,
				 char *pathname, size_t len)
{
	int fd = decompress_kmodule(dso, name, pathname, len);

	close(fd);
	return fd >= 0 ? 0 : -1;
}

 
int __kmod_path__parse(struct kmod_path *m, const char *path,
		       bool alloc_name)
{
	const char *name = strrchr(path, '/');
	const char *ext  = strrchr(path, '.');
	bool is_simple_name = false;

	memset(m, 0x0, sizeof(*m));
	name = name ? name + 1 : path;

	 
	if (name[0] == '[') {
		is_simple_name = true;
		if ((strncmp(name, "[kernel.kallsyms]", 17) == 0) ||
		    (strncmp(name, "[guest.kernel.kallsyms", 22) == 0) ||
		    (strncmp(name, "[vdso]", 6) == 0) ||
		    (strncmp(name, "[vdso32]", 8) == 0) ||
		    (strncmp(name, "[vdsox32]", 9) == 0) ||
		    (strncmp(name, "[vsyscall]", 10) == 0)) {
			m->kmod = false;

		} else
			m->kmod = true;
	}

	 
	if ((ext == NULL) || is_simple_name) {
		if (alloc_name) {
			m->name = strdup(name);
			return m->name ? 0 : -ENOMEM;
		}
		return 0;
	}

	m->comp = is_supported_compression(ext + 1);
	if (m->comp > COMP_ID__NONE)
		ext -= 3;

	 
	if (ext > name)
		m->kmod = !strncmp(ext, ".ko", 3);

	if (alloc_name) {
		if (m->kmod) {
			if (asprintf(&m->name, "[%.*s]", (int) (ext - name), name) == -1)
				return -ENOMEM;
		} else {
			if (asprintf(&m->name, "%s", name) == -1)
				return -ENOMEM;
		}

		strreplace(m->name, '-', '_');
	}

	return 0;
}

void dso__set_module_info(struct dso *dso, struct kmod_path *m,
			  struct machine *machine)
{
	if (machine__is_host(machine))
		dso->symtab_type = DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE;
	else
		dso->symtab_type = DSO_BINARY_TYPE__GUEST_KMODULE;

	 
	if (m->kmod && m->comp) {
		dso->symtab_type++;
		dso->comp = m->comp;
	}

	dso__set_short_name(dso, strdup(m->name), true);
}

 
static LIST_HEAD(dso__data_open);
static long dso__data_open_cnt;
static pthread_mutex_t dso__data_open_lock = PTHREAD_MUTEX_INITIALIZER;

static void dso__list_add(struct dso *dso)
{
	list_add_tail(&dso->data.open_entry, &dso__data_open);
	dso__data_open_cnt++;
}

static void dso__list_del(struct dso *dso)
{
	list_del_init(&dso->data.open_entry);
	WARN_ONCE(dso__data_open_cnt <= 0,
		  "DSO data fd counter out of bounds.");
	dso__data_open_cnt--;
}

static void close_first_dso(void);

static int do_open(char *name)
{
	int fd;
	char sbuf[STRERR_BUFSIZE];

	do {
		fd = open(name, O_RDONLY|O_CLOEXEC);
		if (fd >= 0)
			return fd;

		pr_debug("dso open failed: %s\n",
			 str_error_r(errno, sbuf, sizeof(sbuf)));
		if (!dso__data_open_cnt || errno != EMFILE)
			break;

		close_first_dso();
	} while (1);

	return -1;
}

char *dso__filename_with_chroot(const struct dso *dso, const char *filename)
{
	return filename_with_chroot(nsinfo__pid(dso->nsinfo), filename);
}

static int __open_dso(struct dso *dso, struct machine *machine)
{
	int fd = -EINVAL;
	char *root_dir = (char *)"";
	char *name = malloc(PATH_MAX);
	bool decomp = false;

	if (!name)
		return -ENOMEM;

	mutex_lock(&dso->lock);
	if (machine)
		root_dir = machine->root_dir;

	if (dso__read_binary_type_filename(dso, dso->binary_type,
					    root_dir, name, PATH_MAX))
		goto out;

	if (!is_regular_file(name)) {
		char *new_name;

		if (errno != ENOENT || dso->nsinfo == NULL)
			goto out;

		new_name = dso__filename_with_chroot(dso, name);
		if (!new_name)
			goto out;

		free(name);
		name = new_name;
	}

	if (dso__needs_decompress(dso)) {
		char newpath[KMOD_DECOMP_LEN];
		size_t len = sizeof(newpath);

		if (dso__decompress_kmodule_path(dso, name, newpath, len) < 0) {
			fd = -dso->load_errno;
			goto out;
		}

		decomp = true;
		strcpy(name, newpath);
	}

	fd = do_open(name);

	if (decomp)
		unlink(name);

out:
	mutex_unlock(&dso->lock);
	free(name);
	return fd;
}

static void check_data_close(void);

 
static int open_dso(struct dso *dso, struct machine *machine)
{
	int fd;
	struct nscookie nsc;

	if (dso->binary_type != DSO_BINARY_TYPE__BUILD_ID_CACHE) {
		mutex_lock(&dso->lock);
		nsinfo__mountns_enter(dso->nsinfo, &nsc);
		mutex_unlock(&dso->lock);
	}
	fd = __open_dso(dso, machine);
	if (dso->binary_type != DSO_BINARY_TYPE__BUILD_ID_CACHE)
		nsinfo__mountns_exit(&nsc);

	if (fd >= 0) {
		dso__list_add(dso);
		 
		check_data_close();
	}

	return fd;
}

static void close_data_fd(struct dso *dso)
{
	if (dso->data.fd >= 0) {
		close(dso->data.fd);
		dso->data.fd = -1;
		dso->data.file_size = 0;
		dso__list_del(dso);
	}
}

 
static void close_dso(struct dso *dso)
{
	close_data_fd(dso);
}

static void close_first_dso(void)
{
	struct dso *dso;

	dso = list_first_entry(&dso__data_open, struct dso, data.open_entry);
	close_dso(dso);
}

static rlim_t get_fd_limit(void)
{
	struct rlimit l;
	rlim_t limit = 0;

	 
	if (getrlimit(RLIMIT_NOFILE, &l) == 0) {
		if (l.rlim_cur == RLIM_INFINITY)
			limit = l.rlim_cur;
		else
			limit = l.rlim_cur / 2;
	} else {
		pr_err("failed to get fd limit\n");
		limit = 1;
	}

	return limit;
}

static rlim_t fd_limit;

 
void reset_fd_limit(void)
{
	fd_limit = 0;
}

static bool may_cache_fd(void)
{
	if (!fd_limit)
		fd_limit = get_fd_limit();

	if (fd_limit == RLIM_INFINITY)
		return true;

	return fd_limit > (rlim_t) dso__data_open_cnt;
}

 
static void check_data_close(void)
{
	bool cache_fd = may_cache_fd();

	if (!cache_fd)
		close_first_dso();
}

 
void dso__data_close(struct dso *dso)
{
	pthread_mutex_lock(&dso__data_open_lock);
	close_dso(dso);
	pthread_mutex_unlock(&dso__data_open_lock);
}

static void try_to_open_dso(struct dso *dso, struct machine *machine)
{
	enum dso_binary_type binary_type_data[] = {
		DSO_BINARY_TYPE__BUILD_ID_CACHE,
		DSO_BINARY_TYPE__SYSTEM_PATH_DSO,
		DSO_BINARY_TYPE__NOT_FOUND,
	};
	int i = 0;

	if (dso->data.fd >= 0)
		return;

	if (dso->binary_type != DSO_BINARY_TYPE__NOT_FOUND) {
		dso->data.fd = open_dso(dso, machine);
		goto out;
	}

	do {
		dso->binary_type = binary_type_data[i++];

		dso->data.fd = open_dso(dso, machine);
		if (dso->data.fd >= 0)
			goto out;

	} while (dso->binary_type != DSO_BINARY_TYPE__NOT_FOUND);
out:
	if (dso->data.fd >= 0)
		dso->data.status = DSO_DATA_STATUS_OK;
	else
		dso->data.status = DSO_DATA_STATUS_ERROR;
}

 
int dso__data_get_fd(struct dso *dso, struct machine *machine)
{
	if (dso->data.status == DSO_DATA_STATUS_ERROR)
		return -1;

	if (pthread_mutex_lock(&dso__data_open_lock) < 0)
		return -1;

	try_to_open_dso(dso, machine);

	if (dso->data.fd < 0)
		pthread_mutex_unlock(&dso__data_open_lock);

	return dso->data.fd;
}

void dso__data_put_fd(struct dso *dso __maybe_unused)
{
	pthread_mutex_unlock(&dso__data_open_lock);
}

bool dso__data_status_seen(struct dso *dso, enum dso_data_status_seen by)
{
	u32 flag = 1 << by;

	if (dso->data.status_seen & flag)
		return true;

	dso->data.status_seen |= flag;

	return false;
}

#ifdef HAVE_LIBBPF_SUPPORT
static ssize_t bpf_read(struct dso *dso, u64 offset, char *data)
{
	struct bpf_prog_info_node *node;
	ssize_t size = DSO__DATA_CACHE_SIZE;
	u64 len;
	u8 *buf;

	node = perf_env__find_bpf_prog_info(dso->bpf_prog.env, dso->bpf_prog.id);
	if (!node || !node->info_linear) {
		dso->data.status = DSO_DATA_STATUS_ERROR;
		return -1;
	}

	len = node->info_linear->info.jited_prog_len;
	buf = (u8 *)(uintptr_t)node->info_linear->info.jited_prog_insns;

	if (offset >= len)
		return -1;

	size = (ssize_t)min(len - offset, (u64)size);
	memcpy(data, buf + offset, size);
	return size;
}

static int bpf_size(struct dso *dso)
{
	struct bpf_prog_info_node *node;

	node = perf_env__find_bpf_prog_info(dso->bpf_prog.env, dso->bpf_prog.id);
	if (!node || !node->info_linear) {
		dso->data.status = DSO_DATA_STATUS_ERROR;
		return -1;
	}

	dso->data.file_size = node->info_linear->info.jited_prog_len;
	return 0;
}
#endif 

static void
dso_cache__free(struct dso *dso)
{
	struct rb_root *root = &dso->data.cache;
	struct rb_node *next = rb_first(root);

	mutex_lock(&dso->lock);
	while (next) {
		struct dso_cache *cache;

		cache = rb_entry(next, struct dso_cache, rb_node);
		next = rb_next(&cache->rb_node);
		rb_erase(&cache->rb_node, root);
		free(cache);
	}
	mutex_unlock(&dso->lock);
}

static struct dso_cache *__dso_cache__find(struct dso *dso, u64 offset)
{
	const struct rb_root *root = &dso->data.cache;
	struct rb_node * const *p = &root->rb_node;
	const struct rb_node *parent = NULL;
	struct dso_cache *cache;

	while (*p != NULL) {
		u64 end;

		parent = *p;
		cache = rb_entry(parent, struct dso_cache, rb_node);
		end = cache->offset + DSO__DATA_CACHE_SIZE;

		if (offset < cache->offset)
			p = &(*p)->rb_left;
		else if (offset >= end)
			p = &(*p)->rb_right;
		else
			return cache;
	}

	return NULL;
}

static struct dso_cache *
dso_cache__insert(struct dso *dso, struct dso_cache *new)
{
	struct rb_root *root = &dso->data.cache;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct dso_cache *cache;
	u64 offset = new->offset;

	mutex_lock(&dso->lock);
	while (*p != NULL) {
		u64 end;

		parent = *p;
		cache = rb_entry(parent, struct dso_cache, rb_node);
		end = cache->offset + DSO__DATA_CACHE_SIZE;

		if (offset < cache->offset)
			p = &(*p)->rb_left;
		else if (offset >= end)
			p = &(*p)->rb_right;
		else
			goto out;
	}

	rb_link_node(&new->rb_node, parent, p);
	rb_insert_color(&new->rb_node, root);

	cache = NULL;
out:
	mutex_unlock(&dso->lock);
	return cache;
}

static ssize_t dso_cache__memcpy(struct dso_cache *cache, u64 offset, u8 *data,
				 u64 size, bool out)
{
	u64 cache_offset = offset - cache->offset;
	u64 cache_size   = min(cache->size - cache_offset, size);

	if (out)
		memcpy(data, cache->data + cache_offset, cache_size);
	else
		memcpy(cache->data + cache_offset, data, cache_size);
	return cache_size;
}

static ssize_t file_read(struct dso *dso, struct machine *machine,
			 u64 offset, char *data)
{
	ssize_t ret;

	pthread_mutex_lock(&dso__data_open_lock);

	 
	try_to_open_dso(dso, machine);

	if (dso->data.fd < 0) {
		dso->data.status = DSO_DATA_STATUS_ERROR;
		ret = -errno;
		goto out;
	}

	ret = pread(dso->data.fd, data, DSO__DATA_CACHE_SIZE, offset);
out:
	pthread_mutex_unlock(&dso__data_open_lock);
	return ret;
}

static struct dso_cache *dso_cache__populate(struct dso *dso,
					     struct machine *machine,
					     u64 offset, ssize_t *ret)
{
	u64 cache_offset = offset & DSO__DATA_CACHE_MASK;
	struct dso_cache *cache;
	struct dso_cache *old;

	cache = zalloc(sizeof(*cache) + DSO__DATA_CACHE_SIZE);
	if (!cache) {
		*ret = -ENOMEM;
		return NULL;
	}
#ifdef HAVE_LIBBPF_SUPPORT
	if (dso->binary_type == DSO_BINARY_TYPE__BPF_PROG_INFO)
		*ret = bpf_read(dso, cache_offset, cache->data);
	else
#endif
	if (dso->binary_type == DSO_BINARY_TYPE__OOL)
		*ret = DSO__DATA_CACHE_SIZE;
	else
		*ret = file_read(dso, machine, cache_offset, cache->data);

	if (*ret <= 0) {
		free(cache);
		return NULL;
	}

	cache->offset = cache_offset;
	cache->size   = *ret;

	old = dso_cache__insert(dso, cache);
	if (old) {
		 
		free(cache);
		cache = old;
	}

	return cache;
}

static struct dso_cache *dso_cache__find(struct dso *dso,
					 struct machine *machine,
					 u64 offset,
					 ssize_t *ret)
{
	struct dso_cache *cache = __dso_cache__find(dso, offset);

	return cache ? cache : dso_cache__populate(dso, machine, offset, ret);
}

static ssize_t dso_cache_io(struct dso *dso, struct machine *machine,
			    u64 offset, u8 *data, ssize_t size, bool out)
{
	struct dso_cache *cache;
	ssize_t ret = 0;

	cache = dso_cache__find(dso, machine, offset, &ret);
	if (!cache)
		return ret;

	return dso_cache__memcpy(cache, offset, data, size, out);
}

 
static ssize_t cached_io(struct dso *dso, struct machine *machine,
			 u64 offset, u8 *data, ssize_t size, bool out)
{
	ssize_t r = 0;
	u8 *p = data;

	do {
		ssize_t ret;

		ret = dso_cache_io(dso, machine, offset, p, size, out);
		if (ret < 0)
			return ret;

		 
		if (!ret)
			break;

		BUG_ON(ret > size);

		r      += ret;
		p      += ret;
		offset += ret;
		size   -= ret;

	} while (size);

	return r;
}

static int file_size(struct dso *dso, struct machine *machine)
{
	int ret = 0;
	struct stat st;
	char sbuf[STRERR_BUFSIZE];

	pthread_mutex_lock(&dso__data_open_lock);

	 
	try_to_open_dso(dso, machine);

	if (dso->data.fd < 0) {
		ret = -errno;
		dso->data.status = DSO_DATA_STATUS_ERROR;
		goto out;
	}

	if (fstat(dso->data.fd, &st) < 0) {
		ret = -errno;
		pr_err("dso cache fstat failed: %s\n",
		       str_error_r(errno, sbuf, sizeof(sbuf)));
		dso->data.status = DSO_DATA_STATUS_ERROR;
		goto out;
	}
	dso->data.file_size = st.st_size;

out:
	pthread_mutex_unlock(&dso__data_open_lock);
	return ret;
}

int dso__data_file_size(struct dso *dso, struct machine *machine)
{
	if (dso->data.file_size)
		return 0;

	if (dso->data.status == DSO_DATA_STATUS_ERROR)
		return -1;
#ifdef HAVE_LIBBPF_SUPPORT
	if (dso->binary_type == DSO_BINARY_TYPE__BPF_PROG_INFO)
		return bpf_size(dso);
#endif
	return file_size(dso, machine);
}

 
off_t dso__data_size(struct dso *dso, struct machine *machine)
{
	if (dso__data_file_size(dso, machine))
		return -1;

	 
	return dso->data.file_size;
}

static ssize_t data_read_write_offset(struct dso *dso, struct machine *machine,
				      u64 offset, u8 *data, ssize_t size,
				      bool out)
{
	if (dso__data_file_size(dso, machine))
		return -1;

	 
	if (offset > dso->data.file_size)
		return -1;

	if (offset + size < offset)
		return -1;

	return cached_io(dso, machine, offset, data, size, out);
}

 
ssize_t dso__data_read_offset(struct dso *dso, struct machine *machine,
			      u64 offset, u8 *data, ssize_t size)
{
	if (dso->data.status == DSO_DATA_STATUS_ERROR)
		return -1;

	return data_read_write_offset(dso, machine, offset, data, size, true);
}

 
ssize_t dso__data_read_addr(struct dso *dso, struct map *map,
			    struct machine *machine, u64 addr,
			    u8 *data, ssize_t size)
{
	u64 offset = map__map_ip(map, addr);

	return dso__data_read_offset(dso, machine, offset, data, size);
}

 
ssize_t dso__data_write_cache_offs(struct dso *dso, struct machine *machine,
				   u64 offset, const u8 *data_in, ssize_t size)
{
	u8 *data = (u8 *)data_in;  

	if (dso->data.status == DSO_DATA_STATUS_ERROR)
		return -1;

	return data_read_write_offset(dso, machine, offset, data, size, false);
}

 
ssize_t dso__data_write_cache_addr(struct dso *dso, struct map *map,
				   struct machine *machine, u64 addr,
				   const u8 *data, ssize_t size)
{
	u64 offset = map__map_ip(map, addr);

	return dso__data_write_cache_offs(dso, machine, offset, data, size);
}

struct map *dso__new_map(const char *name)
{
	struct map *map = NULL;
	struct dso *dso = dso__new(name);

	if (dso) {
		map = map__new2(0, dso);
		dso__put(dso);
	}

	return map;
}

struct dso *machine__findnew_kernel(struct machine *machine, const char *name,
				    const char *short_name, int dso_type)
{
	 
	struct dso *dso = machine__findnew_dso(machine, name);

	 
	if (dso != NULL) {
		dso__set_short_name(dso, short_name, false);
		dso->kernel = dso_type;
	}

	return dso;
}

static void dso__set_long_name_id(struct dso *dso, const char *name, struct dso_id *id, bool name_allocated)
{
	struct rb_root *root = dso->root;

	if (name == NULL)
		return;

	if (dso->long_name_allocated)
		free((char *)dso->long_name);

	if (root) {
		rb_erase(&dso->rb_node, root);
		 
		RB_CLEAR_NODE(&dso->rb_node);
		dso->root = NULL;
	}

	dso->long_name		 = name;
	dso->long_name_len	 = strlen(name);
	dso->long_name_allocated = name_allocated;

	if (root)
		__dsos__findnew_link_by_longname_id(root, dso, NULL, id);
}

void dso__set_long_name(struct dso *dso, const char *name, bool name_allocated)
{
	dso__set_long_name_id(dso, name, NULL, name_allocated);
}

void dso__set_short_name(struct dso *dso, const char *name, bool name_allocated)
{
	if (name == NULL)
		return;

	if (dso->short_name_allocated)
		free((char *)dso->short_name);

	dso->short_name		  = name;
	dso->short_name_len	  = strlen(name);
	dso->short_name_allocated = name_allocated;
}

int dso__name_len(const struct dso *dso)
{
	if (!dso)
		return strlen("[unknown]");
	if (verbose > 0)
		return dso->long_name_len;

	return dso->short_name_len;
}

bool dso__loaded(const struct dso *dso)
{
	return dso->loaded;
}

bool dso__sorted_by_name(const struct dso *dso)
{
	return dso->sorted_by_name;
}

void dso__set_sorted_by_name(struct dso *dso)
{
	dso->sorted_by_name = true;
}

struct dso *dso__new_id(const char *name, struct dso_id *id)
{
	struct dso *dso = calloc(1, sizeof(*dso) + strlen(name) + 1);

	if (dso != NULL) {
		strcpy(dso->name, name);
		if (id)
			dso->id = *id;
		dso__set_long_name_id(dso, dso->name, id, false);
		dso__set_short_name(dso, dso->name, false);
		dso->symbols = RB_ROOT_CACHED;
		dso->symbol_names = NULL;
		dso->symbol_names_len = 0;
		dso->data.cache = RB_ROOT;
		dso->inlined_nodes = RB_ROOT_CACHED;
		dso->srclines = RB_ROOT_CACHED;
		dso->data.fd = -1;
		dso->data.status = DSO_DATA_STATUS_UNKNOWN;
		dso->symtab_type = DSO_BINARY_TYPE__NOT_FOUND;
		dso->binary_type = DSO_BINARY_TYPE__NOT_FOUND;
		dso->is_64_bit = (sizeof(void *) == 8);
		dso->loaded = 0;
		dso->rel = 0;
		dso->sorted_by_name = 0;
		dso->has_build_id = 0;
		dso->has_srcline = 1;
		dso->a2l_fails = 1;
		dso->kernel = DSO_SPACE__USER;
		dso->needs_swap = DSO_SWAP__UNSET;
		dso->comp = COMP_ID__NONE;
		RB_CLEAR_NODE(&dso->rb_node);
		dso->root = NULL;
		INIT_LIST_HEAD(&dso->node);
		INIT_LIST_HEAD(&dso->data.open_entry);
		mutex_init(&dso->lock);
		refcount_set(&dso->refcnt, 1);
	}

	return dso;
}

struct dso *dso__new(const char *name)
{
	return dso__new_id(name, NULL);
}

void dso__delete(struct dso *dso)
{
	if (!RB_EMPTY_NODE(&dso->rb_node))
		pr_err("DSO %s is still in rbtree when being deleted!\n",
		       dso->long_name);

	 
	inlines__tree_delete(&dso->inlined_nodes);
	srcline__tree_delete(&dso->srclines);
	symbols__delete(&dso->symbols);
	dso->symbol_names_len = 0;
	zfree(&dso->symbol_names);
	if (dso->short_name_allocated) {
		zfree((char **)&dso->short_name);
		dso->short_name_allocated = false;
	}

	if (dso->long_name_allocated) {
		zfree((char **)&dso->long_name);
		dso->long_name_allocated = false;
	}

	dso__data_close(dso);
	auxtrace_cache__free(dso->auxtrace_cache);
	dso_cache__free(dso);
	dso__free_a2l(dso);
	zfree(&dso->symsrc_filename);
	nsinfo__zput(dso->nsinfo);
	mutex_destroy(&dso->lock);
	free(dso);
}

struct dso *dso__get(struct dso *dso)
{
	if (dso)
		refcount_inc(&dso->refcnt);
	return dso;
}

void dso__put(struct dso *dso)
{
	if (dso && refcount_dec_and_test(&dso->refcnt))
		dso__delete(dso);
}

void dso__set_build_id(struct dso *dso, struct build_id *bid)
{
	dso->bid = *bid;
	dso->has_build_id = 1;
}

bool dso__build_id_equal(const struct dso *dso, struct build_id *bid)
{
	if (dso->bid.size > bid->size && dso->bid.size == BUILD_ID_SIZE) {
		 
		return !memcmp(dso->bid.data, bid->data, bid->size) &&
			!memchr_inv(&dso->bid.data[bid->size], 0,
				    dso->bid.size - bid->size);
	}

	return dso->bid.size == bid->size &&
	       memcmp(dso->bid.data, bid->data, dso->bid.size) == 0;
}

void dso__read_running_kernel_build_id(struct dso *dso, struct machine *machine)
{
	char path[PATH_MAX];

	if (machine__is_default_guest(machine))
		return;
	sprintf(path, "%s/sys/kernel/notes", machine->root_dir);
	if (sysfs__read_build_id(path, &dso->bid) == 0)
		dso->has_build_id = true;
}

int dso__kernel_module_get_build_id(struct dso *dso,
				    const char *root_dir)
{
	char filename[PATH_MAX];
	 
	const char *name = dso->short_name + 1;

	snprintf(filename, sizeof(filename),
		 "%s/sys/module/%.*s/notes/.note.gnu.build-id",
		 root_dir, (int)strlen(name) - 1, name);

	if (sysfs__read_build_id(filename, &dso->bid) == 0)
		dso->has_build_id = true;

	return 0;
}

static size_t dso__fprintf_buildid(struct dso *dso, FILE *fp)
{
	char sbuild_id[SBUILD_ID_SIZE];

	build_id__sprintf(&dso->bid, sbuild_id);
	return fprintf(fp, "%s", sbuild_id);
}

size_t dso__fprintf(struct dso *dso, FILE *fp)
{
	struct rb_node *nd;
	size_t ret = fprintf(fp, "dso: %s (", dso->short_name);

	if (dso->short_name != dso->long_name)
		ret += fprintf(fp, "%s, ", dso->long_name);
	ret += fprintf(fp, "%sloaded, ", dso__loaded(dso) ? "" : "NOT ");
	ret += dso__fprintf_buildid(dso, fp);
	ret += fprintf(fp, ")\n");
	for (nd = rb_first_cached(&dso->symbols); nd; nd = rb_next(nd)) {
		struct symbol *pos = rb_entry(nd, struct symbol, rb_node);
		ret += symbol__fprintf(pos, fp);
	}

	return ret;
}

enum dso_type dso__type(struct dso *dso, struct machine *machine)
{
	int fd;
	enum dso_type type = DSO__TYPE_UNKNOWN;

	fd = dso__data_get_fd(dso, machine);
	if (fd >= 0) {
		type = dso__type_fd(fd);
		dso__data_put_fd(dso);
	}

	return type;
}

int dso__strerror_load(struct dso *dso, char *buf, size_t buflen)
{
	int idx, errnum = dso->load_errno;
	 
	static const char *dso_load__error_str[] = {
	"Internal tools/perf/ library error",
	"Invalid ELF file",
	"Can not read build id",
	"Mismatching build id",
	"Decompression failure",
	};

	BUG_ON(buflen == 0);

	if (errnum >= 0) {
		const char *err = str_error_r(errnum, buf, buflen);

		if (err != buf)
			scnprintf(buf, buflen, "%s", err);

		return 0;
	}

	if (errnum <  __DSO_LOAD_ERRNO__START || errnum >= __DSO_LOAD_ERRNO__END)
		return -1;

	idx = errnum - __DSO_LOAD_ERRNO__START;
	scnprintf(buf, buflen, "%s", dso_load__error_str[idx]);
	return 0;
}
