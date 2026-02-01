
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <regex.h>
#include <stdlib.h>
#include "callchain.h"
#include "debug.h"
#include "dso.h"
#include "env.h"
#include "event.h"
#include "evsel.h"
#include "hist.h"
#include "machine.h"
#include "map.h"
#include "map_symbol.h"
#include "branch.h"
#include "mem-events.h"
#include "path.h"
#include "srcline.h"
#include "symbol.h"
#include "sort.h"
#include "strlist.h"
#include "target.h"
#include "thread.h"
#include "util.h"
#include "vdso.h"
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "unwind.h"
#include "linux/hash.h"
#include "asm/bug.h"
#include "bpf-event.h"
#include <internal/lib.h> 
#include "cgroup.h"
#include "arm64-frame-pointer-unwind-support.h"

#include <linux/ctype.h>
#include <symbol/kallsyms.h>
#include <linux/mman.h>
#include <linux/string.h>
#include <linux/zalloc.h>

static void __machine__remove_thread(struct machine *machine, struct thread_rb_node *nd,
				     struct thread *th, bool lock);

static struct dso *machine__kernel_dso(struct machine *machine)
{
	return map__dso(machine->vmlinux_map);
}

static void dsos__init(struct dsos *dsos)
{
	INIT_LIST_HEAD(&dsos->head);
	dsos->root = RB_ROOT;
	init_rwsem(&dsos->lock);
}

static void machine__threads_init(struct machine *machine)
{
	int i;

	for (i = 0; i < THREADS__TABLE_SIZE; i++) {
		struct threads *threads = &machine->threads[i];
		threads->entries = RB_ROOT_CACHED;
		init_rwsem(&threads->lock);
		threads->nr = 0;
		INIT_LIST_HEAD(&threads->dead);
		threads->last_match = NULL;
	}
}

static int thread_rb_node__cmp_tid(const void *key, const struct rb_node *nd)
{
	int to_find = (int) *((pid_t *)key);

	return to_find - (int)thread__tid(rb_entry(nd, struct thread_rb_node, rb_node)->thread);
}

static struct thread_rb_node *thread_rb_node__find(const struct thread *th,
						   struct rb_root *tree)
{
	pid_t to_find = thread__tid(th);
	struct rb_node *nd = rb_find(&to_find, tree, thread_rb_node__cmp_tid);

	return rb_entry(nd, struct thread_rb_node, rb_node);
}

static int machine__set_mmap_name(struct machine *machine)
{
	if (machine__is_host(machine))
		machine->mmap_name = strdup("[kernel.kallsyms]");
	else if (machine__is_default_guest(machine))
		machine->mmap_name = strdup("[guest.kernel.kallsyms]");
	else if (asprintf(&machine->mmap_name, "[guest.kernel.kallsyms.%d]",
			  machine->pid) < 0)
		machine->mmap_name = NULL;

	return machine->mmap_name ? 0 : -ENOMEM;
}

static void thread__set_guest_comm(struct thread *thread, pid_t pid)
{
	char comm[64];

	snprintf(comm, sizeof(comm), "[guest/%d]", pid);
	thread__set_comm(thread, comm, 0);
}

int machine__init(struct machine *machine, const char *root_dir, pid_t pid)
{
	int err = -ENOMEM;

	memset(machine, 0, sizeof(*machine));
	machine->kmaps = maps__new(machine);
	if (machine->kmaps == NULL)
		return -ENOMEM;

	RB_CLEAR_NODE(&machine->rb_node);
	dsos__init(&machine->dsos);

	machine__threads_init(machine);

	machine->vdso_info = NULL;
	machine->env = NULL;

	machine->pid = pid;

	machine->id_hdr_size = 0;
	machine->kptr_restrict_warned = false;
	machine->comm_exec = false;
	machine->kernel_start = 0;
	machine->vmlinux_map = NULL;

	machine->root_dir = strdup(root_dir);
	if (machine->root_dir == NULL)
		goto out;

	if (machine__set_mmap_name(machine))
		goto out;

	if (pid != HOST_KERNEL_ID) {
		struct thread *thread = machine__findnew_thread(machine, -1,
								pid);

		if (thread == NULL)
			goto out;

		thread__set_guest_comm(thread, pid);
		thread__put(thread);
	}

	machine->current_tid = NULL;
	err = 0;

out:
	if (err) {
		zfree(&machine->kmaps);
		zfree(&machine->root_dir);
		zfree(&machine->mmap_name);
	}
	return 0;
}

struct machine *machine__new_host(void)
{
	struct machine *machine = malloc(sizeof(*machine));

	if (machine != NULL) {
		machine__init(machine, "", HOST_KERNEL_ID);

		if (machine__create_kernel_maps(machine) < 0)
			goto out_delete;
	}

	return machine;
out_delete:
	free(machine);
	return NULL;
}

struct machine *machine__new_kallsyms(void)
{
	struct machine *machine = machine__new_host();
	 
	if (machine && machine__load_kallsyms(machine, "/proc/kallsyms") <= 0) {
		machine__delete(machine);
		machine = NULL;
	}

	return machine;
}

static void dsos__purge(struct dsos *dsos)
{
	struct dso *pos, *n;

	down_write(&dsos->lock);

	list_for_each_entry_safe(pos, n, &dsos->head, node) {
		RB_CLEAR_NODE(&pos->rb_node);
		pos->root = NULL;
		list_del_init(&pos->node);
		dso__put(pos);
	}

	up_write(&dsos->lock);
}

static void dsos__exit(struct dsos *dsos)
{
	dsos__purge(dsos);
	exit_rwsem(&dsos->lock);
}

void machine__delete_threads(struct machine *machine)
{
	struct rb_node *nd;
	int i;

	for (i = 0; i < THREADS__TABLE_SIZE; i++) {
		struct threads *threads = &machine->threads[i];
		down_write(&threads->lock);
		nd = rb_first_cached(&threads->entries);
		while (nd) {
			struct thread_rb_node *trb = rb_entry(nd, struct thread_rb_node, rb_node);

			nd = rb_next(nd);
			__machine__remove_thread(machine, trb, trb->thread, false);
		}
		up_write(&threads->lock);
	}
}

void machine__exit(struct machine *machine)
{
	int i;

	if (machine == NULL)
		return;

	machine__destroy_kernel_maps(machine);
	maps__zput(machine->kmaps);
	dsos__exit(&machine->dsos);
	machine__exit_vdso(machine);
	zfree(&machine->root_dir);
	zfree(&machine->mmap_name);
	zfree(&machine->current_tid);
	zfree(&machine->kallsyms_filename);

	machine__delete_threads(machine);
	for (i = 0; i < THREADS__TABLE_SIZE; i++) {
		struct threads *threads = &machine->threads[i];

		exit_rwsem(&threads->lock);
	}
}

void machine__delete(struct machine *machine)
{
	if (machine) {
		machine__exit(machine);
		free(machine);
	}
}

void machines__init(struct machines *machines)
{
	machine__init(&machines->host, "", HOST_KERNEL_ID);
	machines->guests = RB_ROOT_CACHED;
}

void machines__exit(struct machines *machines)
{
	machine__exit(&machines->host);
	 
}

struct machine *machines__add(struct machines *machines, pid_t pid,
			      const char *root_dir)
{
	struct rb_node **p = &machines->guests.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct machine *pos, *machine = malloc(sizeof(*machine));
	bool leftmost = true;

	if (machine == NULL)
		return NULL;

	if (machine__init(machine, root_dir, pid) != 0) {
		free(machine);
		return NULL;
	}

	while (*p != NULL) {
		parent = *p;
		pos = rb_entry(parent, struct machine, rb_node);
		if (pid < pos->pid)
			p = &(*p)->rb_left;
		else {
			p = &(*p)->rb_right;
			leftmost = false;
		}
	}

	rb_link_node(&machine->rb_node, parent, p);
	rb_insert_color_cached(&machine->rb_node, &machines->guests, leftmost);

	machine->machines = machines;

	return machine;
}

void machines__set_comm_exec(struct machines *machines, bool comm_exec)
{
	struct rb_node *nd;

	machines->host.comm_exec = comm_exec;

	for (nd = rb_first_cached(&machines->guests); nd; nd = rb_next(nd)) {
		struct machine *machine = rb_entry(nd, struct machine, rb_node);

		machine->comm_exec = comm_exec;
	}
}

struct machine *machines__find(struct machines *machines, pid_t pid)
{
	struct rb_node **p = &machines->guests.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct machine *machine;
	struct machine *default_machine = NULL;

	if (pid == HOST_KERNEL_ID)
		return &machines->host;

	while (*p != NULL) {
		parent = *p;
		machine = rb_entry(parent, struct machine, rb_node);
		if (pid < machine->pid)
			p = &(*p)->rb_left;
		else if (pid > machine->pid)
			p = &(*p)->rb_right;
		else
			return machine;
		if (!machine->pid)
			default_machine = machine;
	}

	return default_machine;
}

struct machine *machines__findnew(struct machines *machines, pid_t pid)
{
	char path[PATH_MAX];
	const char *root_dir = "";
	struct machine *machine = machines__find(machines, pid);

	if (machine && (machine->pid == pid))
		goto out;

	if ((pid != HOST_KERNEL_ID) &&
	    (pid != DEFAULT_GUEST_KERNEL_ID) &&
	    (symbol_conf.guestmount)) {
		sprintf(path, "%s/%d", symbol_conf.guestmount, pid);
		if (access(path, R_OK)) {
			static struct strlist *seen;

			if (!seen)
				seen = strlist__new(NULL, NULL);

			if (!strlist__has_entry(seen, path)) {
				pr_err("Can't access file %s\n", path);
				strlist__add(seen, path);
			}
			machine = NULL;
			goto out;
		}
		root_dir = path;
	}

	machine = machines__add(machines, pid, root_dir);
out:
	return machine;
}

struct machine *machines__find_guest(struct machines *machines, pid_t pid)
{
	struct machine *machine = machines__find(machines, pid);

	if (!machine)
		machine = machines__findnew(machines, DEFAULT_GUEST_KERNEL_ID);
	return machine;
}

 
static struct thread *findnew_guest_code(struct machine *machine,
					 struct machine *host_machine,
					 pid_t pid)
{
	struct thread *host_thread;
	struct thread *thread;
	int err;

	if (!machine)
		return NULL;

	thread = machine__findnew_thread(machine, -1, pid);
	if (!thread)
		return NULL;

	 
	if (maps__nr_maps(thread__maps(thread)))
		return thread;

	host_thread = machine__find_thread(host_machine, -1, pid);
	if (!host_thread)
		goto out_err;

	thread__set_guest_comm(thread, pid);

	 
	err = maps__clone(thread, thread__maps(host_thread));
	thread__put(host_thread);
	if (err)
		goto out_err;

	return thread;

out_err:
	thread__zput(thread);
	return NULL;
}

struct thread *machines__findnew_guest_code(struct machines *machines, pid_t pid)
{
	struct machine *host_machine = machines__find(machines, HOST_KERNEL_ID);
	struct machine *machine = machines__findnew(machines, pid);

	return findnew_guest_code(machine, host_machine, pid);
}

struct thread *machine__findnew_guest_code(struct machine *machine, pid_t pid)
{
	struct machines *machines = machine->machines;
	struct machine *host_machine;

	if (!machines)
		return NULL;

	host_machine = machines__find(machines, HOST_KERNEL_ID);

	return findnew_guest_code(machine, host_machine, pid);
}

void machines__process_guests(struct machines *machines,
			      machine__process_t process, void *data)
{
	struct rb_node *nd;

	for (nd = rb_first_cached(&machines->guests); nd; nd = rb_next(nd)) {
		struct machine *pos = rb_entry(nd, struct machine, rb_node);
		process(pos, data);
	}
}

void machines__set_id_hdr_size(struct machines *machines, u16 id_hdr_size)
{
	struct rb_node *node;
	struct machine *machine;

	machines->host.id_hdr_size = id_hdr_size;

	for (node = rb_first_cached(&machines->guests); node;
	     node = rb_next(node)) {
		machine = rb_entry(node, struct machine, rb_node);
		machine->id_hdr_size = id_hdr_size;
	}

	return;
}

static void machine__update_thread_pid(struct machine *machine,
				       struct thread *th, pid_t pid)
{
	struct thread *leader;

	if (pid == thread__pid(th) || pid == -1 || thread__pid(th) != -1)
		return;

	thread__set_pid(th, pid);

	if (thread__pid(th) == thread__tid(th))
		return;

	leader = __machine__findnew_thread(machine, thread__pid(th), thread__pid(th));
	if (!leader)
		goto out_err;

	if (!thread__maps(leader))
		thread__set_maps(leader, maps__new(machine));

	if (!thread__maps(leader))
		goto out_err;

	if (thread__maps(th) == thread__maps(leader))
		goto out_put;

	if (thread__maps(th)) {
		 
		if (!maps__empty(thread__maps(th)))
			pr_err("Discarding thread maps for %d:%d\n",
				thread__pid(th), thread__tid(th));
		maps__put(thread__maps(th));
	}

	thread__set_maps(th, maps__get(thread__maps(leader)));
out_put:
	thread__put(leader);
	return;
out_err:
	pr_err("Failed to join map groups for %d:%d\n", thread__pid(th), thread__tid(th));
	goto out_put;
}

 
static struct thread*
__threads__get_last_match(struct threads *threads, struct machine *machine,
			  int pid, int tid)
{
	struct thread *th;

	th = threads->last_match;
	if (th != NULL) {
		if (thread__tid(th) == tid) {
			machine__update_thread_pid(machine, th, pid);
			return thread__get(th);
		}
		thread__put(threads->last_match);
		threads->last_match = NULL;
	}

	return NULL;
}

static struct thread*
threads__get_last_match(struct threads *threads, struct machine *machine,
			int pid, int tid)
{
	struct thread *th = NULL;

	if (perf_singlethreaded)
		th = __threads__get_last_match(threads, machine, pid, tid);

	return th;
}

static void
__threads__set_last_match(struct threads *threads, struct thread *th)
{
	thread__put(threads->last_match);
	threads->last_match = thread__get(th);
}

static void
threads__set_last_match(struct threads *threads, struct thread *th)
{
	if (perf_singlethreaded)
		__threads__set_last_match(threads, th);
}

 
static struct thread *____machine__findnew_thread(struct machine *machine,
						  struct threads *threads,
						  pid_t pid, pid_t tid,
						  bool create)
{
	struct rb_node **p = &threads->entries.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct thread *th;
	struct thread_rb_node *nd;
	bool leftmost = true;

	th = threads__get_last_match(threads, machine, pid, tid);
	if (th)
		return th;

	while (*p != NULL) {
		parent = *p;
		th = rb_entry(parent, struct thread_rb_node, rb_node)->thread;

		if (thread__tid(th) == tid) {
			threads__set_last_match(threads, th);
			machine__update_thread_pid(machine, th, pid);
			return thread__get(th);
		}

		if (tid < thread__tid(th))
			p = &(*p)->rb_left;
		else {
			p = &(*p)->rb_right;
			leftmost = false;
		}
	}

	if (!create)
		return NULL;

	th = thread__new(pid, tid);
	if (th == NULL)
		return NULL;

	nd = malloc(sizeof(*nd));
	if (nd == NULL) {
		thread__put(th);
		return NULL;
	}
	nd->thread = th;

	rb_link_node(&nd->rb_node, parent, p);
	rb_insert_color_cached(&nd->rb_node, &threads->entries, leftmost);
	 
	if (thread__init_maps(th, machine)) {
		pr_err("Thread init failed thread %d\n", pid);
		rb_erase_cached(&nd->rb_node, &threads->entries);
		RB_CLEAR_NODE(&nd->rb_node);
		free(nd);
		thread__put(th);
		return NULL;
	}
	 
	threads__set_last_match(threads, th);
	++threads->nr;

	return thread__get(th);
}

struct thread *__machine__findnew_thread(struct machine *machine, pid_t pid, pid_t tid)
{
	return ____machine__findnew_thread(machine, machine__threads(machine, tid), pid, tid, true);
}

struct thread *machine__findnew_thread(struct machine *machine, pid_t pid,
				       pid_t tid)
{
	struct threads *threads = machine__threads(machine, tid);
	struct thread *th;

	down_write(&threads->lock);
	th = __machine__findnew_thread(machine, pid, tid);
	up_write(&threads->lock);
	return th;
}

struct thread *machine__find_thread(struct machine *machine, pid_t pid,
				    pid_t tid)
{
	struct threads *threads = machine__threads(machine, tid);
	struct thread *th;

	down_read(&threads->lock);
	th =  ____machine__findnew_thread(machine, threads, pid, tid, false);
	up_read(&threads->lock);
	return th;
}

 
struct thread *machine__idle_thread(struct machine *machine)
{
	struct thread *thread = machine__findnew_thread(machine, 0, 0);

	if (!thread || thread__set_comm(thread, "swapper", 0) ||
	    thread__set_namespaces(thread, 0, NULL))
		pr_err("problem inserting idle task for machine pid %d\n", machine->pid);

	return thread;
}

struct comm *machine__thread_exec_comm(struct machine *machine,
				       struct thread *thread)
{
	if (machine->comm_exec)
		return thread__exec_comm(thread);
	else
		return thread__comm(thread);
}

int machine__process_comm_event(struct machine *machine, union perf_event *event,
				struct perf_sample *sample)
{
	struct thread *thread = machine__findnew_thread(machine,
							event->comm.pid,
							event->comm.tid);
	bool exec = event->header.misc & PERF_RECORD_MISC_COMM_EXEC;
	int err = 0;

	if (exec)
		machine->comm_exec = true;

	if (dump_trace)
		perf_event__fprintf_comm(event, stdout);

	if (thread == NULL ||
	    __thread__set_comm(thread, event->comm.comm, sample->time, exec)) {
		dump_printf("problem processing PERF_RECORD_COMM, skipping event.\n");
		err = -1;
	}

	thread__put(thread);

	return err;
}

int machine__process_namespaces_event(struct machine *machine __maybe_unused,
				      union perf_event *event,
				      struct perf_sample *sample __maybe_unused)
{
	struct thread *thread = machine__findnew_thread(machine,
							event->namespaces.pid,
							event->namespaces.tid);
	int err = 0;

	WARN_ONCE(event->namespaces.nr_namespaces > NR_NAMESPACES,
		  "\nWARNING: kernel seems to support more namespaces than perf"
		  " tool.\nTry updating the perf tool..\n\n");

	WARN_ONCE(event->namespaces.nr_namespaces < NR_NAMESPACES,
		  "\nWARNING: perf tool seems to support more namespaces than"
		  " the kernel.\nTry updating the kernel..\n\n");

	if (dump_trace)
		perf_event__fprintf_namespaces(event, stdout);

	if (thread == NULL ||
	    thread__set_namespaces(thread, sample->time, &event->namespaces)) {
		dump_printf("problem processing PERF_RECORD_NAMESPACES, skipping event.\n");
		err = -1;
	}

	thread__put(thread);

	return err;
}

int machine__process_cgroup_event(struct machine *machine,
				  union perf_event *event,
				  struct perf_sample *sample __maybe_unused)
{
	struct cgroup *cgrp;

	if (dump_trace)
		perf_event__fprintf_cgroup(event, stdout);

	cgrp = cgroup__findnew(machine->env, event->cgroup.id, event->cgroup.path);
	if (cgrp == NULL)
		return -ENOMEM;

	return 0;
}

int machine__process_lost_event(struct machine *machine __maybe_unused,
				union perf_event *event, struct perf_sample *sample __maybe_unused)
{
	dump_printf(": id:%" PRI_lu64 ": lost:%" PRI_lu64 "\n",
		    event->lost.id, event->lost.lost);
	return 0;
}

int machine__process_lost_samples_event(struct machine *machine __maybe_unused,
					union perf_event *event, struct perf_sample *sample)
{
	dump_printf(": id:%" PRIu64 ": lost samples :%" PRI_lu64 "\n",
		    sample->id, event->lost_samples.lost);
	return 0;
}

static struct dso *machine__findnew_module_dso(struct machine *machine,
					       struct kmod_path *m,
					       const char *filename)
{
	struct dso *dso;

	down_write(&machine->dsos.lock);

	dso = __dsos__find(&machine->dsos, m->name, true);
	if (!dso) {
		dso = __dsos__addnew(&machine->dsos, m->name);
		if (dso == NULL)
			goto out_unlock;

		dso__set_module_info(dso, m, machine);
		dso__set_long_name(dso, strdup(filename), true);
		dso->kernel = DSO_SPACE__KERNEL;
	}

	dso__get(dso);
out_unlock:
	up_write(&machine->dsos.lock);
	return dso;
}

int machine__process_aux_event(struct machine *machine __maybe_unused,
			       union perf_event *event)
{
	if (dump_trace)
		perf_event__fprintf_aux(event, stdout);
	return 0;
}

int machine__process_itrace_start_event(struct machine *machine __maybe_unused,
					union perf_event *event)
{
	if (dump_trace)
		perf_event__fprintf_itrace_start(event, stdout);
	return 0;
}

int machine__process_aux_output_hw_id_event(struct machine *machine __maybe_unused,
					    union perf_event *event)
{
	if (dump_trace)
		perf_event__fprintf_aux_output_hw_id(event, stdout);
	return 0;
}

int machine__process_switch_event(struct machine *machine __maybe_unused,
				  union perf_event *event)
{
	if (dump_trace)
		perf_event__fprintf_switch(event, stdout);
	return 0;
}

static int machine__process_ksymbol_register(struct machine *machine,
					     union perf_event *event,
					     struct perf_sample *sample __maybe_unused)
{
	struct symbol *sym;
	struct dso *dso;
	struct map *map = maps__find(machine__kernel_maps(machine), event->ksymbol.addr);
	bool put_map = false;
	int err = 0;

	if (!map) {
		dso = dso__new(event->ksymbol.name);

		if (!dso) {
			err = -ENOMEM;
			goto out;
		}
		dso->kernel = DSO_SPACE__KERNEL;
		map = map__new2(0, dso);
		dso__put(dso);
		if (!map) {
			err = -ENOMEM;
			goto out;
		}
		 
		put_map = true;
		if (event->ksymbol.ksym_type == PERF_RECORD_KSYMBOL_TYPE_OOL) {
			dso->binary_type = DSO_BINARY_TYPE__OOL;
			dso->data.file_size = event->ksymbol.len;
			dso__set_loaded(dso);
		}

		map__set_start(map, event->ksymbol.addr);
		map__set_end(map, map__start(map) + event->ksymbol.len);
		err = maps__insert(machine__kernel_maps(machine), map);
		if (err) {
			err = -ENOMEM;
			goto out;
		}

		dso__set_loaded(dso);

		if (is_bpf_image(event->ksymbol.name)) {
			dso->binary_type = DSO_BINARY_TYPE__BPF_IMAGE;
			dso__set_long_name(dso, "", false);
		}
	} else {
		dso = map__dso(map);
	}

	sym = symbol__new(map__map_ip(map, map__start(map)),
			  event->ksymbol.len,
			  0, 0, event->ksymbol.name);
	if (!sym) {
		err = -ENOMEM;
		goto out;
	}
	dso__insert_symbol(dso, sym);
out:
	if (put_map)
		map__put(map);
	return err;
}

static int machine__process_ksymbol_unregister(struct machine *machine,
					       union perf_event *event,
					       struct perf_sample *sample __maybe_unused)
{
	struct symbol *sym;
	struct map *map;

	map = maps__find(machine__kernel_maps(machine), event->ksymbol.addr);
	if (!map)
		return 0;

	if (RC_CHK_ACCESS(map) != RC_CHK_ACCESS(machine->vmlinux_map))
		maps__remove(machine__kernel_maps(machine), map);
	else {
		struct dso *dso = map__dso(map);

		sym = dso__find_symbol(dso, map__map_ip(map, map__start(map)));
		if (sym)
			dso__delete_symbol(dso, sym);
	}

	return 0;
}

int machine__process_ksymbol(struct machine *machine __maybe_unused,
			     union perf_event *event,
			     struct perf_sample *sample)
{
	if (dump_trace)
		perf_event__fprintf_ksymbol(event, stdout);

	if (event->ksymbol.flags & PERF_RECORD_KSYMBOL_FLAGS_UNREGISTER)
		return machine__process_ksymbol_unregister(machine, event,
							   sample);
	return machine__process_ksymbol_register(machine, event, sample);
}

int machine__process_text_poke(struct machine *machine, union perf_event *event,
			       struct perf_sample *sample __maybe_unused)
{
	struct map *map = maps__find(machine__kernel_maps(machine), event->text_poke.addr);
	u8 cpumode = event->header.misc & PERF_RECORD_MISC_CPUMODE_MASK;
	struct dso *dso = map ? map__dso(map) : NULL;

	if (dump_trace)
		perf_event__fprintf_text_poke(event, machine, stdout);

	if (!event->text_poke.new_len)
		return 0;

	if (cpumode != PERF_RECORD_MISC_KERNEL) {
		pr_debug("%s: unsupported cpumode - ignoring\n", __func__);
		return 0;
	}

	if (dso) {
		u8 *new_bytes = event->text_poke.bytes + event->text_poke.old_len;
		int ret;

		 
		map__load(map);
		ret = dso__data_write_cache_addr(dso, map, machine,
						 event->text_poke.addr,
						 new_bytes,
						 event->text_poke.new_len);
		if (ret != event->text_poke.new_len)
			pr_debug("Failed to write kernel text poke at %#" PRI_lx64 "\n",
				 event->text_poke.addr);
	} else {
		pr_debug("Failed to find kernel text poke address map for %#" PRI_lx64 "\n",
			 event->text_poke.addr);
	}

	return 0;
}

static struct map *machine__addnew_module_map(struct machine *machine, u64 start,
					      const char *filename)
{
	struct map *map = NULL;
	struct kmod_path m;
	struct dso *dso;
	int err;

	if (kmod_path__parse_name(&m, filename))
		return NULL;

	dso = machine__findnew_module_dso(machine, &m, filename);
	if (dso == NULL)
		goto out;

	map = map__new2(start, dso);
	if (map == NULL)
		goto out;

	err = maps__insert(machine__kernel_maps(machine), map);
	 
	if (err) {
		map__put(map);
		map = NULL;
	}
out:
	 
	dso__put(dso);
	zfree(&m.name);
	return map;
}

size_t machines__fprintf_dsos(struct machines *machines, FILE *fp)
{
	struct rb_node *nd;
	size_t ret = __dsos__fprintf(&machines->host.dsos.head, fp);

	for (nd = rb_first_cached(&machines->guests); nd; nd = rb_next(nd)) {
		struct machine *pos = rb_entry(nd, struct machine, rb_node);
		ret += __dsos__fprintf(&pos->dsos.head, fp);
	}

	return ret;
}

size_t machine__fprintf_dsos_buildid(struct machine *m, FILE *fp,
				     bool (skip)(struct dso *dso, int parm), int parm)
{
	return __dsos__fprintf_buildid(&m->dsos.head, fp, skip, parm);
}

size_t machines__fprintf_dsos_buildid(struct machines *machines, FILE *fp,
				     bool (skip)(struct dso *dso, int parm), int parm)
{
	struct rb_node *nd;
	size_t ret = machine__fprintf_dsos_buildid(&machines->host, fp, skip, parm);

	for (nd = rb_first_cached(&machines->guests); nd; nd = rb_next(nd)) {
		struct machine *pos = rb_entry(nd, struct machine, rb_node);
		ret += machine__fprintf_dsos_buildid(pos, fp, skip, parm);
	}
	return ret;
}

size_t machine__fprintf_vmlinux_path(struct machine *machine, FILE *fp)
{
	int i;
	size_t printed = 0;
	struct dso *kdso = machine__kernel_dso(machine);

	if (kdso->has_build_id) {
		char filename[PATH_MAX];
		if (dso__build_id_filename(kdso, filename, sizeof(filename),
					   false))
			printed += fprintf(fp, "[0] %s\n", filename);
	}

	for (i = 0; i < vmlinux_path__nr_entries; ++i)
		printed += fprintf(fp, "[%d] %s\n",
				   i + kdso->has_build_id, vmlinux_path[i]);

	return printed;
}

size_t machine__fprintf(struct machine *machine, FILE *fp)
{
	struct rb_node *nd;
	size_t ret;
	int i;

	for (i = 0; i < THREADS__TABLE_SIZE; i++) {
		struct threads *threads = &machine->threads[i];

		down_read(&threads->lock);

		ret = fprintf(fp, "Threads: %u\n", threads->nr);

		for (nd = rb_first_cached(&threads->entries); nd;
		     nd = rb_next(nd)) {
			struct thread *pos = rb_entry(nd, struct thread_rb_node, rb_node)->thread;

			ret += thread__fprintf(pos, fp);
		}

		up_read(&threads->lock);
	}
	return ret;
}

static struct dso *machine__get_kernel(struct machine *machine)
{
	const char *vmlinux_name = machine->mmap_name;
	struct dso *kernel;

	if (machine__is_host(machine)) {
		if (symbol_conf.vmlinux_name)
			vmlinux_name = symbol_conf.vmlinux_name;

		kernel = machine__findnew_kernel(machine, vmlinux_name,
						 "[kernel]", DSO_SPACE__KERNEL);
	} else {
		if (symbol_conf.default_guest_vmlinux_name)
			vmlinux_name = symbol_conf.default_guest_vmlinux_name;

		kernel = machine__findnew_kernel(machine, vmlinux_name,
						 "[guest.kernel]",
						 DSO_SPACE__KERNEL_GUEST);
	}

	if (kernel != NULL && (!kernel->has_build_id))
		dso__read_running_kernel_build_id(kernel, machine);

	return kernel;
}

void machine__get_kallsyms_filename(struct machine *machine, char *buf,
				    size_t bufsz)
{
	if (machine__is_default_guest(machine))
		scnprintf(buf, bufsz, "%s", symbol_conf.default_guest_kallsyms);
	else
		scnprintf(buf, bufsz, "%s/proc/kallsyms", machine->root_dir);
}

const char *ref_reloc_sym_names[] = {"_text", "_stext", NULL};

 
static int machine__get_running_kernel_start(struct machine *machine,
					     const char **symbol_name,
					     u64 *start, u64 *end)
{
	char filename[PATH_MAX];
	int i, err = -1;
	const char *name;
	u64 addr = 0;

	machine__get_kallsyms_filename(machine, filename, PATH_MAX);

	if (symbol__restricted_filename(filename, "/proc/kallsyms"))
		return 0;

	for (i = 0; (name = ref_reloc_sym_names[i]) != NULL; i++) {
		err = kallsyms__get_function_start(filename, name, &addr);
		if (!err)
			break;
	}

	if (err)
		return -1;

	if (symbol_name)
		*symbol_name = name;

	*start = addr;

	err = kallsyms__get_symbol_start(filename, "_edata", &addr);
	if (err)
		err = kallsyms__get_function_start(filename, "_etext", &addr);
	if (!err)
		*end = addr;

	return 0;
}

int machine__create_extra_kernel_map(struct machine *machine,
				     struct dso *kernel,
				     struct extra_kernel_map *xm)
{
	struct kmap *kmap;
	struct map *map;
	int err;

	map = map__new2(xm->start, kernel);
	if (!map)
		return -ENOMEM;

	map__set_end(map, xm->end);
	map__set_pgoff(map, xm->pgoff);

	kmap = map__kmap(map);

	strlcpy(kmap->name, xm->name, KMAP_NAME_LEN);

	err = maps__insert(machine__kernel_maps(machine), map);

	if (!err) {
		pr_debug2("Added extra kernel map %s %" PRIx64 "-%" PRIx64 "\n",
			kmap->name, map__start(map), map__end(map));
	}

	map__put(map);

	return err;
}

static u64 find_entry_trampoline(struct dso *dso)
{
	 
	const char *syms[] = {
		"_entry_trampoline",
		"__entry_trampoline_start",
		"entry_SYSCALL_64_trampoline",
	};
	struct symbol *sym = dso__first_symbol(dso);
	unsigned int i;

	for (; sym; sym = dso__next_symbol(sym)) {
		if (sym->binding != STB_GLOBAL)
			continue;
		for (i = 0; i < ARRAY_SIZE(syms); i++) {
			if (!strcmp(sym->name, syms[i]))
				return sym->start;
		}
	}

	return 0;
}

 
#define X86_64_CPU_ENTRY_AREA_PER_CPU	0xfffffe0000000000ULL
#define X86_64_CPU_ENTRY_AREA_SIZE	0x2c000
#define X86_64_ENTRY_TRAMPOLINE		0x6000

 
int machine__map_x86_64_entry_trampolines(struct machine *machine,
					  struct dso *kernel)
{
	struct maps *kmaps = machine__kernel_maps(machine);
	int nr_cpus_avail, cpu;
	bool found = false;
	struct map_rb_node *rb_node;
	u64 pgoff;

	 
	maps__for_each_entry(kmaps, rb_node) {
		struct map *dest_map, *map = rb_node->map;
		struct kmap *kmap = __map__kmap(map);

		if (!kmap || !is_entry_trampoline(kmap->name))
			continue;

		dest_map = maps__find(kmaps, map__pgoff(map));
		if (dest_map != map)
			map__set_pgoff(map, map__map_ip(dest_map, map__pgoff(map)));
		found = true;
	}
	if (found || machine->trampolines_mapped)
		return 0;

	pgoff = find_entry_trampoline(kernel);
	if (!pgoff)
		return 0;

	nr_cpus_avail = machine__nr_cpus_avail(machine);

	 
	for (cpu = 0; cpu < nr_cpus_avail; cpu++) {
		u64 va = X86_64_CPU_ENTRY_AREA_PER_CPU +
			 cpu * X86_64_CPU_ENTRY_AREA_SIZE +
			 X86_64_ENTRY_TRAMPOLINE;
		struct extra_kernel_map xm = {
			.start = va,
			.end   = va + page_size,
			.pgoff = pgoff,
		};

		strlcpy(xm.name, ENTRY_TRAMPOLINE_NAME, KMAP_NAME_LEN);

		if (machine__create_extra_kernel_map(machine, kernel, &xm) < 0)
			return -1;
	}

	machine->trampolines_mapped = nr_cpus_avail;

	return 0;
}

int __weak machine__create_extra_kernel_maps(struct machine *machine __maybe_unused,
					     struct dso *kernel __maybe_unused)
{
	return 0;
}

static int
__machine__create_kernel_maps(struct machine *machine, struct dso *kernel)
{
	 
	machine__destroy_kernel_maps(machine);

	map__put(machine->vmlinux_map);
	machine->vmlinux_map = map__new2(0, kernel);
	if (machine->vmlinux_map == NULL)
		return -ENOMEM;

	map__set_map_ip(machine->vmlinux_map, identity__map_ip);
	map__set_unmap_ip(machine->vmlinux_map, identity__map_ip);
	return maps__insert(machine__kernel_maps(machine), machine->vmlinux_map);
}

void machine__destroy_kernel_maps(struct machine *machine)
{
	struct kmap *kmap;
	struct map *map = machine__kernel_map(machine);

	if (map == NULL)
		return;

	kmap = map__kmap(map);
	maps__remove(machine__kernel_maps(machine), map);
	if (kmap && kmap->ref_reloc_sym) {
		zfree((char **)&kmap->ref_reloc_sym->name);
		zfree(&kmap->ref_reloc_sym);
	}

	map__zput(machine->vmlinux_map);
}

int machines__create_guest_kernel_maps(struct machines *machines)
{
	int ret = 0;
	struct dirent **namelist = NULL;
	int i, items = 0;
	char path[PATH_MAX];
	pid_t pid;
	char *endp;

	if (symbol_conf.default_guest_vmlinux_name ||
	    symbol_conf.default_guest_modules ||
	    symbol_conf.default_guest_kallsyms) {
		machines__create_kernel_maps(machines, DEFAULT_GUEST_KERNEL_ID);
	}

	if (symbol_conf.guestmount) {
		items = scandir(symbol_conf.guestmount, &namelist, NULL, NULL);
		if (items <= 0)
			return -ENOENT;
		for (i = 0; i < items; i++) {
			if (!isdigit(namelist[i]->d_name[0])) {
				 
				continue;
			}
			pid = (pid_t)strtol(namelist[i]->d_name, &endp, 10);
			if ((*endp != '\0') ||
			    (endp == namelist[i]->d_name) ||
			    (errno == ERANGE)) {
				pr_debug("invalid directory (%s). Skipping.\n",
					 namelist[i]->d_name);
				continue;
			}
			sprintf(path, "%s/%s/proc/kallsyms",
				symbol_conf.guestmount,
				namelist[i]->d_name);
			ret = access(path, R_OK);
			if (ret) {
				pr_debug("Can't access file %s\n", path);
				goto failure;
			}
			machines__create_kernel_maps(machines, pid);
		}
failure:
		free(namelist);
	}

	return ret;
}

void machines__destroy_kernel_maps(struct machines *machines)
{
	struct rb_node *next = rb_first_cached(&machines->guests);

	machine__destroy_kernel_maps(&machines->host);

	while (next) {
		struct machine *pos = rb_entry(next, struct machine, rb_node);

		next = rb_next(&pos->rb_node);
		rb_erase_cached(&pos->rb_node, &machines->guests);
		machine__delete(pos);
	}
}

int machines__create_kernel_maps(struct machines *machines, pid_t pid)
{
	struct machine *machine = machines__findnew(machines, pid);

	if (machine == NULL)
		return -1;

	return machine__create_kernel_maps(machine);
}

int machine__load_kallsyms(struct machine *machine, const char *filename)
{
	struct map *map = machine__kernel_map(machine);
	struct dso *dso = map__dso(map);
	int ret = __dso__load_kallsyms(dso, filename, map, true);

	if (ret > 0) {
		dso__set_loaded(dso);
		 
		maps__fixup_end(machine__kernel_maps(machine));
	}

	return ret;
}

int machine__load_vmlinux_path(struct machine *machine)
{
	struct map *map = machine__kernel_map(machine);
	struct dso *dso = map__dso(map);
	int ret = dso__load_vmlinux_path(dso, map);

	if (ret > 0)
		dso__set_loaded(dso);

	return ret;
}

static char *get_kernel_version(const char *root_dir)
{
	char version[PATH_MAX];
	FILE *file;
	char *name, *tmp;
	const char *prefix = "Linux version ";

	sprintf(version, "%s/proc/version", root_dir);
	file = fopen(version, "r");
	if (!file)
		return NULL;

	tmp = fgets(version, sizeof(version), file);
	fclose(file);
	if (!tmp)
		return NULL;

	name = strstr(version, prefix);
	if (!name)
		return NULL;
	name += strlen(prefix);
	tmp = strchr(name, ' ');
	if (tmp)
		*tmp = '\0';

	return strdup(name);
}

static bool is_kmod_dso(struct dso *dso)
{
	return dso->symtab_type == DSO_BINARY_TYPE__SYSTEM_PATH_KMODULE ||
	       dso->symtab_type == DSO_BINARY_TYPE__GUEST_KMODULE;
}

static int maps__set_module_path(struct maps *maps, const char *path, struct kmod_path *m)
{
	char *long_name;
	struct dso *dso;
	struct map *map = maps__find_by_name(maps, m->name);

	if (map == NULL)
		return 0;

	long_name = strdup(path);
	if (long_name == NULL)
		return -ENOMEM;

	dso = map__dso(map);
	dso__set_long_name(dso, long_name, true);
	dso__kernel_module_get_build_id(dso, "");

	 
	if (m->comp && is_kmod_dso(dso)) {
		dso->symtab_type++;
		dso->comp = m->comp;
	}

	return 0;
}

static int maps__set_modules_path_dir(struct maps *maps, const char *dir_name, int depth)
{
	struct dirent *dent;
	DIR *dir = opendir(dir_name);
	int ret = 0;

	if (!dir) {
		pr_debug("%s: cannot open %s dir\n", __func__, dir_name);
		return -1;
	}

	while ((dent = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		struct stat st;

		 
		path__join(path, sizeof(path), dir_name, dent->d_name);
		if (stat(path, &st))
			continue;

		if (S_ISDIR(st.st_mode)) {
			if (!strcmp(dent->d_name, ".") ||
			    !strcmp(dent->d_name, ".."))
				continue;

			 
			if (depth == 0) {
				if (!strcmp(dent->d_name, "source") ||
				    !strcmp(dent->d_name, "build"))
					continue;
			}

			ret = maps__set_modules_path_dir(maps, path, depth + 1);
			if (ret < 0)
				goto out;
		} else {
			struct kmod_path m;

			ret = kmod_path__parse_name(&m, dent->d_name);
			if (ret)
				goto out;

			if (m.kmod)
				ret = maps__set_module_path(maps, path, &m);

			zfree(&m.name);

			if (ret)
				goto out;
		}
	}

out:
	closedir(dir);
	return ret;
}

static int machine__set_modules_path(struct machine *machine)
{
	char *version;
	char modules_path[PATH_MAX];

	version = get_kernel_version(machine->root_dir);
	if (!version)
		return -1;

	snprintf(modules_path, sizeof(modules_path), "%s/lib/modules/%s",
		 machine->root_dir, version);
	free(version);

	return maps__set_modules_path_dir(machine__kernel_maps(machine), modules_path, 0);
}
int __weak arch__fix_module_text_start(u64 *start __maybe_unused,
				u64 *size __maybe_unused,
				const char *name __maybe_unused)
{
	return 0;
}

static int machine__create_module(void *arg, const char *name, u64 start,
				  u64 size)
{
	struct machine *machine = arg;
	struct map *map;

	if (arch__fix_module_text_start(&start, &size, name) < 0)
		return -1;

	map = machine__addnew_module_map(machine, start, name);
	if (map == NULL)
		return -1;
	map__set_end(map, start + size);

	dso__kernel_module_get_build_id(map__dso(map), machine->root_dir);
	map__put(map);
	return 0;
}

static int machine__create_modules(struct machine *machine)
{
	const char *modules;
	char path[PATH_MAX];

	if (machine__is_default_guest(machine)) {
		modules = symbol_conf.default_guest_modules;
	} else {
		snprintf(path, PATH_MAX, "%s/proc/modules", machine->root_dir);
		modules = path;
	}

	if (symbol__restricted_filename(modules, "/proc/modules"))
		return -1;

	if (modules__parse(modules, machine, machine__create_module))
		return -1;

	if (!machine__set_modules_path(machine))
		return 0;

	pr_debug("Problems setting modules path maps, continuing anyway...\n");

	return 0;
}

static void machine__set_kernel_mmap(struct machine *machine,
				     u64 start, u64 end)
{
	map__set_start(machine->vmlinux_map, start);
	map__set_end(machine->vmlinux_map, end);
	 
	if (start == 0 && end == 0)
		map__set_end(machine->vmlinux_map, ~0ULL);
}

static int machine__update_kernel_mmap(struct machine *machine,
				     u64 start, u64 end)
{
	struct map *orig, *updated;
	int err;

	orig = machine->vmlinux_map;
	updated = map__get(orig);

	machine->vmlinux_map = updated;
	machine__set_kernel_mmap(machine, start, end);
	maps__remove(machine__kernel_maps(machine), orig);
	err = maps__insert(machine__kernel_maps(machine), updated);
	map__put(orig);

	return err;
}

int machine__create_kernel_maps(struct machine *machine)
{
	struct dso *kernel = machine__get_kernel(machine);
	const char *name = NULL;
	u64 start = 0, end = ~0ULL;
	int ret;

	if (kernel == NULL)
		return -1;

	ret = __machine__create_kernel_maps(machine, kernel);
	if (ret < 0)
		goto out_put;

	if (symbol_conf.use_modules && machine__create_modules(machine) < 0) {
		if (machine__is_host(machine))
			pr_debug("Problems creating module maps, "
				 "continuing anyway...\n");
		else
			pr_debug("Problems creating module maps for guest %d, "
				 "continuing anyway...\n", machine->pid);
	}

	if (!machine__get_running_kernel_start(machine, &name, &start, &end)) {
		if (name &&
		    map__set_kallsyms_ref_reloc_sym(machine->vmlinux_map, name, start)) {
			machine__destroy_kernel_maps(machine);
			ret = -1;
			goto out_put;
		}

		 
		ret = machine__update_kernel_mmap(machine, start, end);
		if (ret < 0)
			goto out_put;
	}

	if (machine__create_extra_kernel_maps(machine, kernel))
		pr_debug("Problems creating extra kernel maps, continuing anyway...\n");

	if (end == ~0ULL) {
		 
		struct map_rb_node *rb_node = maps__find_node(machine__kernel_maps(machine),
							machine__kernel_map(machine));
		struct map_rb_node *next = map_rb_node__next(rb_node);

		if (next)
			machine__set_kernel_mmap(machine, start, map__start(next->map));
	}

out_put:
	dso__put(kernel);
	return ret;
}

static bool machine__uses_kcore(struct machine *machine)
{
	struct dso *dso;

	list_for_each_entry(dso, &machine->dsos.head, node) {
		if (dso__is_kcore(dso))
			return true;
	}

	return false;
}

static bool perf_event__is_extra_kernel_mmap(struct machine *machine,
					     struct extra_kernel_map *xm)
{
	return machine__is(machine, "x86_64") &&
	       is_entry_trampoline(xm->name);
}

static int machine__process_extra_kernel_map(struct machine *machine,
					     struct extra_kernel_map *xm)
{
	struct dso *kernel = machine__kernel_dso(machine);

	if (kernel == NULL)
		return -1;

	return machine__create_extra_kernel_map(machine, kernel, xm);
}

static int machine__process_kernel_mmap_event(struct machine *machine,
					      struct extra_kernel_map *xm,
					      struct build_id *bid)
{
	enum dso_space_type dso_space;
	bool is_kernel_mmap;
	const char *mmap_name = machine->mmap_name;

	 
	if (machine__uses_kcore(machine))
		return 0;

	if (machine__is_host(machine))
		dso_space = DSO_SPACE__KERNEL;
	else
		dso_space = DSO_SPACE__KERNEL_GUEST;

	is_kernel_mmap = memcmp(xm->name, mmap_name, strlen(mmap_name) - 1) == 0;
	if (!is_kernel_mmap && !machine__is_host(machine)) {
		 
		mmap_name = "[kernel.kallsyms]";
		is_kernel_mmap = memcmp(xm->name, mmap_name, strlen(mmap_name) - 1) == 0;
	}
	if (xm->name[0] == '/' ||
	    (!is_kernel_mmap && xm->name[0] == '[')) {
		struct map *map = machine__addnew_module_map(machine, xm->start, xm->name);

		if (map == NULL)
			goto out_problem;

		map__set_end(map, map__start(map) + xm->end - xm->start);

		if (build_id__is_defined(bid))
			dso__set_build_id(map__dso(map), bid);

		map__put(map);
	} else if (is_kernel_mmap) {
		const char *symbol_name = xm->name + strlen(mmap_name);
		 
		struct dso *kernel = NULL;
		struct dso *dso;

		down_read(&machine->dsos.lock);

		list_for_each_entry(dso, &machine->dsos.head, node) {

			 

			if (!dso->kernel ||
			    is_kernel_module(dso->long_name,
					     PERF_RECORD_MISC_CPUMODE_UNKNOWN))
				continue;


			kernel = dso__get(dso);
			break;
		}

		up_read(&machine->dsos.lock);

		if (kernel == NULL)
			kernel = machine__findnew_dso(machine, machine->mmap_name);
		if (kernel == NULL)
			goto out_problem;

		kernel->kernel = dso_space;
		if (__machine__create_kernel_maps(machine, kernel) < 0) {
			dso__put(kernel);
			goto out_problem;
		}

		if (strstr(kernel->long_name, "vmlinux"))
			dso__set_short_name(kernel, "[kernel.vmlinux]", false);

		if (machine__update_kernel_mmap(machine, xm->start, xm->end) < 0) {
			dso__put(kernel);
			goto out_problem;
		}

		if (build_id__is_defined(bid))
			dso__set_build_id(kernel, bid);

		 
		if (xm->pgoff != 0) {
			map__set_kallsyms_ref_reloc_sym(machine->vmlinux_map,
							symbol_name,
							xm->pgoff);
		}

		if (machine__is_default_guest(machine)) {
			 
			dso__load(kernel, machine__kernel_map(machine));
		}
		dso__put(kernel);
	} else if (perf_event__is_extra_kernel_mmap(machine, xm)) {
		return machine__process_extra_kernel_map(machine, xm);
	}
	return 0;
out_problem:
	return -1;
}

int machine__process_mmap2_event(struct machine *machine,
				 union perf_event *event,
				 struct perf_sample *sample)
{
	struct thread *thread;
	struct map *map;
	struct dso_id dso_id = {
		.maj = event->mmap2.maj,
		.min = event->mmap2.min,
		.ino = event->mmap2.ino,
		.ino_generation = event->mmap2.ino_generation,
	};
	struct build_id __bid, *bid = NULL;
	int ret = 0;

	if (dump_trace)
		perf_event__fprintf_mmap2(event, stdout);

	if (event->header.misc & PERF_RECORD_MISC_MMAP_BUILD_ID) {
		bid = &__bid;
		build_id__init(bid, event->mmap2.build_id, event->mmap2.build_id_size);
	}

	if (sample->cpumode == PERF_RECORD_MISC_GUEST_KERNEL ||
	    sample->cpumode == PERF_RECORD_MISC_KERNEL) {
		struct extra_kernel_map xm = {
			.start = event->mmap2.start,
			.end   = event->mmap2.start + event->mmap2.len,
			.pgoff = event->mmap2.pgoff,
		};

		strlcpy(xm.name, event->mmap2.filename, KMAP_NAME_LEN);
		ret = machine__process_kernel_mmap_event(machine, &xm, bid);
		if (ret < 0)
			goto out_problem;
		return 0;
	}

	thread = machine__findnew_thread(machine, event->mmap2.pid,
					event->mmap2.tid);
	if (thread == NULL)
		goto out_problem;

	map = map__new(machine, event->mmap2.start,
			event->mmap2.len, event->mmap2.pgoff,
			&dso_id, event->mmap2.prot,
			event->mmap2.flags, bid,
			event->mmap2.filename, thread);

	if (map == NULL)
		goto out_problem_map;

	ret = thread__insert_map(thread, map);
	if (ret)
		goto out_problem_insert;

	thread__put(thread);
	map__put(map);
	return 0;

out_problem_insert:
	map__put(map);
out_problem_map:
	thread__put(thread);
out_problem:
	dump_printf("problem processing PERF_RECORD_MMAP2, skipping event.\n");
	return 0;
}

int machine__process_mmap_event(struct machine *machine, union perf_event *event,
				struct perf_sample *sample)
{
	struct thread *thread;
	struct map *map;
	u32 prot = 0;
	int ret = 0;

	if (dump_trace)
		perf_event__fprintf_mmap(event, stdout);

	if (sample->cpumode == PERF_RECORD_MISC_GUEST_KERNEL ||
	    sample->cpumode == PERF_RECORD_MISC_KERNEL) {
		struct extra_kernel_map xm = {
			.start = event->mmap.start,
			.end   = event->mmap.start + event->mmap.len,
			.pgoff = event->mmap.pgoff,
		};

		strlcpy(xm.name, event->mmap.filename, KMAP_NAME_LEN);
		ret = machine__process_kernel_mmap_event(machine, &xm, NULL);
		if (ret < 0)
			goto out_problem;
		return 0;
	}

	thread = machine__findnew_thread(machine, event->mmap.pid,
					 event->mmap.tid);
	if (thread == NULL)
		goto out_problem;

	if (!(event->header.misc & PERF_RECORD_MISC_MMAP_DATA))
		prot = PROT_EXEC;

	map = map__new(machine, event->mmap.start,
			event->mmap.len, event->mmap.pgoff,
			NULL, prot, 0, NULL, event->mmap.filename, thread);

	if (map == NULL)
		goto out_problem_map;

	ret = thread__insert_map(thread, map);
	if (ret)
		goto out_problem_insert;

	thread__put(thread);
	map__put(map);
	return 0;

out_problem_insert:
	map__put(map);
out_problem_map:
	thread__put(thread);
out_problem:
	dump_printf("problem processing PERF_RECORD_MMAP, skipping event.\n");
	return 0;
}

static void __machine__remove_thread(struct machine *machine, struct thread_rb_node *nd,
				     struct thread *th, bool lock)
{
	struct threads *threads = machine__threads(machine, thread__tid(th));

	if (!nd)
		nd = thread_rb_node__find(th, &threads->entries.rb_root);

	if (threads->last_match && RC_CHK_ACCESS(threads->last_match) == RC_CHK_ACCESS(th))
		threads__set_last_match(threads, NULL);

	if (lock)
		down_write(&threads->lock);

	BUG_ON(refcount_read(thread__refcnt(th)) == 0);

	thread__put(nd->thread);
	rb_erase_cached(&nd->rb_node, &threads->entries);
	RB_CLEAR_NODE(&nd->rb_node);
	--threads->nr;

	free(nd);

	if (lock)
		up_write(&threads->lock);
}

void machine__remove_thread(struct machine *machine, struct thread *th)
{
	return __machine__remove_thread(machine, NULL, th, true);
}

int machine__process_fork_event(struct machine *machine, union perf_event *event,
				struct perf_sample *sample)
{
	struct thread *thread = machine__find_thread(machine,
						     event->fork.pid,
						     event->fork.tid);
	struct thread *parent = machine__findnew_thread(machine,
							event->fork.ppid,
							event->fork.ptid);
	bool do_maps_clone = true;
	int err = 0;

	if (dump_trace)
		perf_event__fprintf_task(event, stdout);

	 
	if (thread__pid(parent) != (pid_t)event->fork.ppid) {
		dump_printf("removing erroneous parent thread %d/%d\n",
			    thread__pid(parent), thread__tid(parent));
		machine__remove_thread(machine, parent);
		thread__put(parent);
		parent = machine__findnew_thread(machine, event->fork.ppid,
						 event->fork.ptid);
	}

	 
	if (thread != NULL) {
		machine__remove_thread(machine, thread);
		thread__put(thread);
	}

	thread = machine__findnew_thread(machine, event->fork.pid,
					 event->fork.tid);
	 
	if (event->fork.header.misc & PERF_RECORD_MISC_FORK_EXEC)
		do_maps_clone = false;

	if (thread == NULL || parent == NULL ||
	    thread__fork(thread, parent, sample->time, do_maps_clone) < 0) {
		dump_printf("problem processing PERF_RECORD_FORK, skipping event.\n");
		err = -1;
	}
	thread__put(thread);
	thread__put(parent);

	return err;
}

int machine__process_exit_event(struct machine *machine, union perf_event *event,
				struct perf_sample *sample __maybe_unused)
{
	struct thread *thread = machine__find_thread(machine,
						     event->fork.pid,
						     event->fork.tid);

	if (dump_trace)
		perf_event__fprintf_task(event, stdout);

	if (thread != NULL)
		thread__put(thread);

	return 0;
}

int machine__process_event(struct machine *machine, union perf_event *event,
			   struct perf_sample *sample)
{
	int ret;

	switch (event->header.type) {
	case PERF_RECORD_COMM:
		ret = machine__process_comm_event(machine, event, sample); break;
	case PERF_RECORD_MMAP:
		ret = machine__process_mmap_event(machine, event, sample); break;
	case PERF_RECORD_NAMESPACES:
		ret = machine__process_namespaces_event(machine, event, sample); break;
	case PERF_RECORD_CGROUP:
		ret = machine__process_cgroup_event(machine, event, sample); break;
	case PERF_RECORD_MMAP2:
		ret = machine__process_mmap2_event(machine, event, sample); break;
	case PERF_RECORD_FORK:
		ret = machine__process_fork_event(machine, event, sample); break;
	case PERF_RECORD_EXIT:
		ret = machine__process_exit_event(machine, event, sample); break;
	case PERF_RECORD_LOST:
		ret = machine__process_lost_event(machine, event, sample); break;
	case PERF_RECORD_AUX:
		ret = machine__process_aux_event(machine, event); break;
	case PERF_RECORD_ITRACE_START:
		ret = machine__process_itrace_start_event(machine, event); break;
	case PERF_RECORD_LOST_SAMPLES:
		ret = machine__process_lost_samples_event(machine, event, sample); break;
	case PERF_RECORD_SWITCH:
	case PERF_RECORD_SWITCH_CPU_WIDE:
		ret = machine__process_switch_event(machine, event); break;
	case PERF_RECORD_KSYMBOL:
		ret = machine__process_ksymbol(machine, event, sample); break;
	case PERF_RECORD_BPF_EVENT:
		ret = machine__process_bpf(machine, event, sample); break;
	case PERF_RECORD_TEXT_POKE:
		ret = machine__process_text_poke(machine, event, sample); break;
	case PERF_RECORD_AUX_OUTPUT_HW_ID:
		ret = machine__process_aux_output_hw_id_event(machine, event); break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static bool symbol__match_regex(struct symbol *sym, regex_t *regex)
{
	if (!regexec(regex, sym->name, 0, NULL, 0))
		return true;
	return false;
}

static void ip__resolve_ams(struct thread *thread,
			    struct addr_map_symbol *ams,
			    u64 ip)
{
	struct addr_location al;

	addr_location__init(&al);
	 
	thread__find_cpumode_addr_location(thread, ip, &al);

	ams->addr = ip;
	ams->al_addr = al.addr;
	ams->al_level = al.level;
	ams->ms.maps = maps__get(al.maps);
	ams->ms.sym = al.sym;
	ams->ms.map = map__get(al.map);
	ams->phys_addr = 0;
	ams->data_page_size = 0;
	addr_location__exit(&al);
}

static void ip__resolve_data(struct thread *thread,
			     u8 m, struct addr_map_symbol *ams,
			     u64 addr, u64 phys_addr, u64 daddr_page_size)
{
	struct addr_location al;

	addr_location__init(&al);

	thread__find_symbol(thread, m, addr, &al);

	ams->addr = addr;
	ams->al_addr = al.addr;
	ams->al_level = al.level;
	ams->ms.maps = maps__get(al.maps);
	ams->ms.sym = al.sym;
	ams->ms.map = map__get(al.map);
	ams->phys_addr = phys_addr;
	ams->data_page_size = daddr_page_size;
	addr_location__exit(&al);
}

struct mem_info *sample__resolve_mem(struct perf_sample *sample,
				     struct addr_location *al)
{
	struct mem_info *mi = mem_info__new();

	if (!mi)
		return NULL;

	ip__resolve_ams(al->thread, &mi->iaddr, sample->ip);
	ip__resolve_data(al->thread, al->cpumode, &mi->daddr,
			 sample->addr, sample->phys_addr,
			 sample->data_page_size);
	mi->data_src.val = sample->data_src;

	return mi;
}

static char *callchain_srcline(struct map_symbol *ms, u64 ip)
{
	struct map *map = ms->map;
	char *srcline = NULL;
	struct dso *dso;

	if (!map || callchain_param.key == CCKEY_FUNCTION)
		return srcline;

	dso = map__dso(map);
	srcline = srcline__tree_find(&dso->srclines, ip);
	if (!srcline) {
		bool show_sym = false;
		bool show_addr = callchain_param.key == CCKEY_ADDRESS;

		srcline = get_srcline(dso, map__rip_2objdump(map, ip),
				      ms->sym, show_sym, show_addr, ip);
		srcline__tree_insert(&dso->srclines, ip, srcline);
	}

	return srcline;
}

struct iterations {
	int nr_loop_iter;
	u64 cycles;
};

static int add_callchain_ip(struct thread *thread,
			    struct callchain_cursor *cursor,
			    struct symbol **parent,
			    struct addr_location *root_al,
			    u8 *cpumode,
			    u64 ip,
			    bool branch,
			    struct branch_flags *flags,
			    struct iterations *iter,
			    u64 branch_from)
{
	struct map_symbol ms = {};
	struct addr_location al;
	int nr_loop_iter = 0, err = 0;
	u64 iter_cycles = 0;
	const char *srcline = NULL;

	addr_location__init(&al);
	al.filtered = 0;
	al.sym = NULL;
	al.srcline = NULL;
	if (!cpumode) {
		thread__find_cpumode_addr_location(thread, ip, &al);
	} else {
		if (ip >= PERF_CONTEXT_MAX) {
			switch (ip) {
			case PERF_CONTEXT_HV:
				*cpumode = PERF_RECORD_MISC_HYPERVISOR;
				break;
			case PERF_CONTEXT_KERNEL:
				*cpumode = PERF_RECORD_MISC_KERNEL;
				break;
			case PERF_CONTEXT_USER:
				*cpumode = PERF_RECORD_MISC_USER;
				break;
			default:
				pr_debug("invalid callchain context: "
					 "%"PRId64"\n", (s64) ip);
				 
				callchain_cursor_reset(cursor);
				err = 1;
				goto out;
			}
			goto out;
		}
		thread__find_symbol(thread, *cpumode, ip, &al);
	}

	if (al.sym != NULL) {
		if (perf_hpp_list.parent && !*parent &&
		    symbol__match_regex(al.sym, &parent_regex))
			*parent = al.sym;
		else if (have_ignore_callees && root_al &&
		  symbol__match_regex(al.sym, &ignore_callees_regex)) {
			 
			addr_location__copy(root_al, &al);
			callchain_cursor_reset(cursor);
		}
	}

	if (symbol_conf.hide_unresolved && al.sym == NULL)
		goto out;

	if (iter) {
		nr_loop_iter = iter->nr_loop_iter;
		iter_cycles = iter->cycles;
	}

	ms.maps = maps__get(al.maps);
	ms.map = map__get(al.map);
	ms.sym = al.sym;
	srcline = callchain_srcline(&ms, al.addr);
	err = callchain_cursor_append(cursor, ip, &ms,
				      branch, flags, nr_loop_iter,
				      iter_cycles, branch_from, srcline);
out:
	addr_location__exit(&al);
	maps__put(ms.maps);
	map__put(ms.map);
	return err;
}

struct branch_info *sample__resolve_bstack(struct perf_sample *sample,
					   struct addr_location *al)
{
	unsigned int i;
	const struct branch_stack *bs = sample->branch_stack;
	struct branch_entry *entries = perf_sample__branch_entries(sample);
	struct branch_info *bi = calloc(bs->nr, sizeof(struct branch_info));

	if (!bi)
		return NULL;

	for (i = 0; i < bs->nr; i++) {
		ip__resolve_ams(al->thread, &bi[i].to, entries[i].to);
		ip__resolve_ams(al->thread, &bi[i].from, entries[i].from);
		bi[i].flags = entries[i].flags;
	}
	return bi;
}

static void save_iterations(struct iterations *iter,
			    struct branch_entry *be, int nr)
{
	int i;

	iter->nr_loop_iter++;
	iter->cycles = 0;

	for (i = 0; i < nr; i++)
		iter->cycles += be[i].flags.cycles;
}

#define CHASHSZ 127
#define CHASHBITS 7
#define NO_ENTRY 0xff

#define PERF_MAX_BRANCH_DEPTH 127

 
static int remove_loops(struct branch_entry *l, int nr,
			struct iterations *iter)
{
	int i, j, off;
	unsigned char chash[CHASHSZ];

	memset(chash, NO_ENTRY, sizeof(chash));

	BUG_ON(PERF_MAX_BRANCH_DEPTH > 255);

	for (i = 0; i < nr; i++) {
		int h = hash_64(l[i].from, CHASHBITS) % CHASHSZ;

		 
		if (chash[h] == NO_ENTRY) {
			chash[h] = i;
		} else if (l[chash[h]].from == l[i].from) {
			bool is_loop = true;
			 
			off = 0;
			for (j = chash[h]; j < i && i + off < nr; j++, off++)
				if (l[j].from != l[i + off].from) {
					is_loop = false;
					break;
				}
			if (is_loop) {
				j = nr - (i + off);
				if (j > 0) {
					save_iterations(iter + i + off,
						l + i, off);

					memmove(iter + i, iter + i + off,
						j * sizeof(*iter));

					memmove(l + i, l + i + off,
						j * sizeof(*l));
				}

				nr -= off;
			}
		}
	}
	return nr;
}

static int lbr_callchain_add_kernel_ip(struct thread *thread,
				       struct callchain_cursor *cursor,
				       struct perf_sample *sample,
				       struct symbol **parent,
				       struct addr_location *root_al,
				       u64 branch_from,
				       bool callee, int end)
{
	struct ip_callchain *chain = sample->callchain;
	u8 cpumode = PERF_RECORD_MISC_USER;
	int err, i;

	if (callee) {
		for (i = 0; i < end + 1; i++) {
			err = add_callchain_ip(thread, cursor, parent,
					       root_al, &cpumode, chain->ips[i],
					       false, NULL, NULL, branch_from);
			if (err)
				return err;
		}
		return 0;
	}

	for (i = end; i >= 0; i--) {
		err = add_callchain_ip(thread, cursor, parent,
				       root_al, &cpumode, chain->ips[i],
				       false, NULL, NULL, branch_from);
		if (err)
			return err;
	}

	return 0;
}

static void save_lbr_cursor_node(struct thread *thread,
				 struct callchain_cursor *cursor,
				 int idx)
{
	struct lbr_stitch *lbr_stitch = thread__lbr_stitch(thread);

	if (!lbr_stitch)
		return;

	if (cursor->pos == cursor->nr) {
		lbr_stitch->prev_lbr_cursor[idx].valid = false;
		return;
	}

	if (!cursor->curr)
		cursor->curr = cursor->first;
	else
		cursor->curr = cursor->curr->next;
	memcpy(&lbr_stitch->prev_lbr_cursor[idx], cursor->curr,
	       sizeof(struct callchain_cursor_node));

	lbr_stitch->prev_lbr_cursor[idx].valid = true;
	cursor->pos++;
}

static int lbr_callchain_add_lbr_ip(struct thread *thread,
				    struct callchain_cursor *cursor,
				    struct perf_sample *sample,
				    struct symbol **parent,
				    struct addr_location *root_al,
				    u64 *branch_from,
				    bool callee)
{
	struct branch_stack *lbr_stack = sample->branch_stack;
	struct branch_entry *entries = perf_sample__branch_entries(sample);
	u8 cpumode = PERF_RECORD_MISC_USER;
	int lbr_nr = lbr_stack->nr;
	struct branch_flags *flags;
	int err, i;
	u64 ip;

	 
	if (thread__lbr_stitch(thread)) {
		cursor->curr = NULL;
		cursor->pos = cursor->nr;
		if (cursor->nr) {
			cursor->curr = cursor->first;
			for (i = 0; i < (int)(cursor->nr - 1); i++)
				cursor->curr = cursor->curr->next;
		}
	}

	if (callee) {
		 
		ip = entries[0].to;
		flags = &entries[0].flags;
		*branch_from = entries[0].from;
		err = add_callchain_ip(thread, cursor, parent,
				       root_al, &cpumode, ip,
				       true, flags, NULL,
				       *branch_from);
		if (err)
			return err;

		 
		if (thread__lbr_stitch(thread) && (cursor->pos != cursor->nr)) {
			if (!cursor->curr)
				cursor->curr = cursor->first;
			else
				cursor->curr = cursor->curr->next;
			cursor->pos++;
		}

		 
		for (i = 0; i < lbr_nr; i++) {
			ip = entries[i].from;
			flags = &entries[i].flags;
			err = add_callchain_ip(thread, cursor, parent,
					       root_al, &cpumode, ip,
					       true, flags, NULL,
					       *branch_from);
			if (err)
				return err;
			save_lbr_cursor_node(thread, cursor, i);
		}
		return 0;
	}

	 
	for (i = lbr_nr - 1; i >= 0; i--) {
		ip = entries[i].from;
		flags = &entries[i].flags;
		err = add_callchain_ip(thread, cursor, parent,
				       root_al, &cpumode, ip,
				       true, flags, NULL,
				       *branch_from);
		if (err)
			return err;
		save_lbr_cursor_node(thread, cursor, i);
	}

	if (lbr_nr > 0) {
		 
		ip = entries[0].to;
		flags = &entries[0].flags;
		*branch_from = entries[0].from;
		err = add_callchain_ip(thread, cursor, parent,
				root_al, &cpumode, ip,
				true, flags, NULL,
				*branch_from);
		if (err)
			return err;
	}

	return 0;
}

static int lbr_callchain_add_stitched_lbr_ip(struct thread *thread,
					     struct callchain_cursor *cursor)
{
	struct lbr_stitch *lbr_stitch = thread__lbr_stitch(thread);
	struct callchain_cursor_node *cnode;
	struct stitch_list *stitch_node;
	int err;

	list_for_each_entry(stitch_node, &lbr_stitch->lists, node) {
		cnode = &stitch_node->cursor;

		err = callchain_cursor_append(cursor, cnode->ip,
					      &cnode->ms,
					      cnode->branch,
					      &cnode->branch_flags,
					      cnode->nr_loop_iter,
					      cnode->iter_cycles,
					      cnode->branch_from,
					      cnode->srcline);
		if (err)
			return err;
	}
	return 0;
}

static struct stitch_list *get_stitch_node(struct thread *thread)
{
	struct lbr_stitch *lbr_stitch = thread__lbr_stitch(thread);
	struct stitch_list *stitch_node;

	if (!list_empty(&lbr_stitch->free_lists)) {
		stitch_node = list_first_entry(&lbr_stitch->free_lists,
					       struct stitch_list, node);
		list_del(&stitch_node->node);

		return stitch_node;
	}

	return malloc(sizeof(struct stitch_list));
}

static bool has_stitched_lbr(struct thread *thread,
			     struct perf_sample *cur,
			     struct perf_sample *prev,
			     unsigned int max_lbr,
			     bool callee)
{
	struct branch_stack *cur_stack = cur->branch_stack;
	struct branch_entry *cur_entries = perf_sample__branch_entries(cur);
	struct branch_stack *prev_stack = prev->branch_stack;
	struct branch_entry *prev_entries = perf_sample__branch_entries(prev);
	struct lbr_stitch *lbr_stitch = thread__lbr_stitch(thread);
	int i, j, nr_identical_branches = 0;
	struct stitch_list *stitch_node;
	u64 cur_base, distance;

	if (!cur_stack || !prev_stack)
		return false;

	 
	cur_base = max_lbr - cur_stack->nr + cur_stack->hw_idx + 1;

	distance = (prev_stack->hw_idx > cur_base) ? (prev_stack->hw_idx - cur_base) :
						     (max_lbr + prev_stack->hw_idx - cur_base);
	 
	if (distance + 1 > prev_stack->nr)
		return false;

	 
	for (i = distance, j = cur_stack->nr - 1; (i >= 0) && (j >= 0); i--, j--) {
		if ((prev_entries[i].from != cur_entries[j].from) ||
		    (prev_entries[i].to != cur_entries[j].to) ||
		    (prev_entries[i].flags.value != cur_entries[j].flags.value))
			break;
		nr_identical_branches++;
	}

	if (!nr_identical_branches)
		return false;

	 
	for (i = prev_stack->nr - 1; i > (int)distance; i--) {

		if (!lbr_stitch->prev_lbr_cursor[i].valid)
			continue;

		stitch_node = get_stitch_node(thread);
		if (!stitch_node)
			return false;

		memcpy(&stitch_node->cursor, &lbr_stitch->prev_lbr_cursor[i],
		       sizeof(struct callchain_cursor_node));

		if (callee)
			list_add(&stitch_node->node, &lbr_stitch->lists);
		else
			list_add_tail(&stitch_node->node, &lbr_stitch->lists);
	}

	return true;
}

static bool alloc_lbr_stitch(struct thread *thread, unsigned int max_lbr)
{
	if (thread__lbr_stitch(thread))
		return true;

	thread__set_lbr_stitch(thread, zalloc(sizeof(struct lbr_stitch)));
	if (!thread__lbr_stitch(thread))
		goto err;

	thread__lbr_stitch(thread)->prev_lbr_cursor =
		calloc(max_lbr + 1, sizeof(struct callchain_cursor_node));
	if (!thread__lbr_stitch(thread)->prev_lbr_cursor)
		goto free_lbr_stitch;

	INIT_LIST_HEAD(&thread__lbr_stitch(thread)->lists);
	INIT_LIST_HEAD(&thread__lbr_stitch(thread)->free_lists);

	return true;

free_lbr_stitch:
	free(thread__lbr_stitch(thread));
	thread__set_lbr_stitch(thread, NULL);
err:
	pr_warning("Failed to allocate space for stitched LBRs. Disable LBR stitch\n");
	thread__set_lbr_stitch_enable(thread, false);
	return false;
}

 
static int resolve_lbr_callchain_sample(struct thread *thread,
					struct callchain_cursor *cursor,
					struct perf_sample *sample,
					struct symbol **parent,
					struct addr_location *root_al,
					int max_stack,
					unsigned int max_lbr)
{
	bool callee = (callchain_param.order == ORDER_CALLEE);
	struct ip_callchain *chain = sample->callchain;
	int chain_nr = min(max_stack, (int)chain->nr), i;
	struct lbr_stitch *lbr_stitch;
	bool stitched_lbr = false;
	u64 branch_from = 0;
	int err;

	for (i = 0; i < chain_nr; i++) {
		if (chain->ips[i] == PERF_CONTEXT_USER)
			break;
	}

	 
	if (i == chain_nr)
		return 0;

	if (thread__lbr_stitch_enable(thread) && !sample->no_hw_idx &&
	    (max_lbr > 0) && alloc_lbr_stitch(thread, max_lbr)) {
		lbr_stitch = thread__lbr_stitch(thread);

		stitched_lbr = has_stitched_lbr(thread, sample,
						&lbr_stitch->prev_sample,
						max_lbr, callee);

		if (!stitched_lbr && !list_empty(&lbr_stitch->lists)) {
			list_replace_init(&lbr_stitch->lists,
					  &lbr_stitch->free_lists);
		}
		memcpy(&lbr_stitch->prev_sample, sample, sizeof(*sample));
	}

	if (callee) {
		 
		err = lbr_callchain_add_kernel_ip(thread, cursor, sample,
						  parent, root_al, branch_from,
						  true, i);
		if (err)
			goto error;

		err = lbr_callchain_add_lbr_ip(thread, cursor, sample, parent,
					       root_al, &branch_from, true);
		if (err)
			goto error;

		if (stitched_lbr) {
			err = lbr_callchain_add_stitched_lbr_ip(thread, cursor);
			if (err)
				goto error;
		}

	} else {
		if (stitched_lbr) {
			err = lbr_callchain_add_stitched_lbr_ip(thread, cursor);
			if (err)
				goto error;
		}
		err = lbr_callchain_add_lbr_ip(thread, cursor, sample, parent,
					       root_al, &branch_from, false);
		if (err)
			goto error;

		 
		err = lbr_callchain_add_kernel_ip(thread, cursor, sample,
						  parent, root_al, branch_from,
						  false, i);
		if (err)
			goto error;
	}
	return 1;

error:
	return (err < 0) ? err : 0;
}

static int find_prev_cpumode(struct ip_callchain *chain, struct thread *thread,
			     struct callchain_cursor *cursor,
			     struct symbol **parent,
			     struct addr_location *root_al,
			     u8 *cpumode, int ent)
{
	int err = 0;

	while (--ent >= 0) {
		u64 ip = chain->ips[ent];

		if (ip >= PERF_CONTEXT_MAX) {
			err = add_callchain_ip(thread, cursor, parent,
					       root_al, cpumode, ip,
					       false, NULL, NULL, 0);
			break;
		}
	}
	return err;
}

static u64 get_leaf_frame_caller(struct perf_sample *sample,
		struct thread *thread, int usr_idx)
{
	if (machine__normalized_is(maps__machine(thread__maps(thread)), "arm64"))
		return get_leaf_frame_caller_aarch64(sample, thread, usr_idx);
	else
		return 0;
}

static int thread__resolve_callchain_sample(struct thread *thread,
					    struct callchain_cursor *cursor,
					    struct evsel *evsel,
					    struct perf_sample *sample,
					    struct symbol **parent,
					    struct addr_location *root_al,
					    int max_stack)
{
	struct branch_stack *branch = sample->branch_stack;
	struct branch_entry *entries = perf_sample__branch_entries(sample);
	struct ip_callchain *chain = sample->callchain;
	int chain_nr = 0;
	u8 cpumode = PERF_RECORD_MISC_USER;
	int i, j, err, nr_entries, usr_idx;
	int skip_idx = -1;
	int first_call = 0;
	u64 leaf_frame_caller;

	if (chain)
		chain_nr = chain->nr;

	if (evsel__has_branch_callstack(evsel)) {
		struct perf_env *env = evsel__env(evsel);

		err = resolve_lbr_callchain_sample(thread, cursor, sample, parent,
						   root_al, max_stack,
						   !env ? 0 : env->max_branches);
		if (err)
			return (err < 0) ? err : 0;
	}

	 
	skip_idx = arch_skip_callchain_idx(thread, chain);

	 

	if (branch && callchain_param.branch_callstack) {
		int nr = min(max_stack, (int)branch->nr);
		struct branch_entry be[nr];
		struct iterations iter[nr];

		if (branch->nr > PERF_MAX_BRANCH_DEPTH) {
			pr_warning("corrupted branch chain. skipping...\n");
			goto check_calls;
		}

		for (i = 0; i < nr; i++) {
			if (callchain_param.order == ORDER_CALLEE) {
				be[i] = entries[i];

				if (chain == NULL)
					continue;

				 
				if (i == skip_idx ||
				    chain->ips[first_call] >= PERF_CONTEXT_MAX)
					first_call++;
				else if (be[i].from < chain->ips[first_call] &&
				    be[i].from >= chain->ips[first_call] - 8)
					first_call++;
			} else
				be[i] = entries[branch->nr - i - 1];
		}

		memset(iter, 0, sizeof(struct iterations) * nr);
		nr = remove_loops(be, nr, iter);

		for (i = 0; i < nr; i++) {
			err = add_callchain_ip(thread, cursor, parent,
					       root_al,
					       NULL, be[i].to,
					       true, &be[i].flags,
					       NULL, be[i].from);

			if (!err)
				err = add_callchain_ip(thread, cursor, parent, root_al,
						       NULL, be[i].from,
						       true, &be[i].flags,
						       &iter[i], 0);
			if (err == -EINVAL)
				break;
			if (err)
				return err;
		}

		if (chain_nr == 0)
			return 0;

		chain_nr -= nr;
	}

check_calls:
	if (chain && callchain_param.order != ORDER_CALLEE) {
		err = find_prev_cpumode(chain, thread, cursor, parent, root_al,
					&cpumode, chain->nr - first_call);
		if (err)
			return (err < 0) ? err : 0;
	}
	for (i = first_call, nr_entries = 0;
	     i < chain_nr && nr_entries < max_stack; i++) {
		u64 ip;

		if (callchain_param.order == ORDER_CALLEE)
			j = i;
		else
			j = chain->nr - i - 1;

#ifdef HAVE_SKIP_CALLCHAIN_IDX
		if (j == skip_idx)
			continue;
#endif
		ip = chain->ips[j];
		if (ip < PERF_CONTEXT_MAX)
                       ++nr_entries;
		else if (callchain_param.order != ORDER_CALLEE) {
			err = find_prev_cpumode(chain, thread, cursor, parent,
						root_al, &cpumode, j);
			if (err)
				return (err < 0) ? err : 0;
			continue;
		}

		 

		usr_idx = callchain_param.order == ORDER_CALLEE ? j-2 : j-1;

		if (usr_idx >= 0 && chain->ips[usr_idx] == PERF_CONTEXT_USER) {

			leaf_frame_caller = get_leaf_frame_caller(sample, thread, usr_idx);

			 

			if (leaf_frame_caller && leaf_frame_caller != ip) {

				err = add_callchain_ip(thread, cursor, parent,
					       root_al, &cpumode, leaf_frame_caller,
					       false, NULL, NULL, 0);
				if (err)
					return (err < 0) ? err : 0;
			}
		}

		err = add_callchain_ip(thread, cursor, parent,
				       root_al, &cpumode, ip,
				       false, NULL, NULL, 0);

		if (err)
			return (err < 0) ? err : 0;
	}

	return 0;
}

static int append_inlines(struct callchain_cursor *cursor, struct map_symbol *ms, u64 ip)
{
	struct symbol *sym = ms->sym;
	struct map *map = ms->map;
	struct inline_node *inline_node;
	struct inline_list *ilist;
	struct dso *dso;
	u64 addr;
	int ret = 1;
	struct map_symbol ilist_ms;

	if (!symbol_conf.inline_name || !map || !sym)
		return ret;

	addr = map__dso_map_ip(map, ip);
	addr = map__rip_2objdump(map, addr);
	dso = map__dso(map);

	inline_node = inlines__tree_find(&dso->inlined_nodes, addr);
	if (!inline_node) {
		inline_node = dso__parse_addr_inlines(dso, addr, sym);
		if (!inline_node)
			return ret;
		inlines__tree_insert(&dso->inlined_nodes, inline_node);
	}

	ilist_ms = (struct map_symbol) {
		.maps = maps__get(ms->maps),
		.map = map__get(map),
	};
	list_for_each_entry(ilist, &inline_node->val, list) {
		ilist_ms.sym = ilist->symbol;
		ret = callchain_cursor_append(cursor, ip, &ilist_ms, false,
					      NULL, 0, 0, 0, ilist->srcline);

		if (ret != 0)
			return ret;
	}
	map__put(ilist_ms.map);
	maps__put(ilist_ms.maps);

	return ret;
}

static int unwind_entry(struct unwind_entry *entry, void *arg)
{
	struct callchain_cursor *cursor = arg;
	const char *srcline = NULL;
	u64 addr = entry->ip;

	if (symbol_conf.hide_unresolved && entry->ms.sym == NULL)
		return 0;

	if (append_inlines(cursor, &entry->ms, entry->ip) == 0)
		return 0;

	 
	if (entry->ms.map)
		addr = map__dso_map_ip(entry->ms.map, entry->ip);

	srcline = callchain_srcline(&entry->ms, addr);
	return callchain_cursor_append(cursor, entry->ip, &entry->ms,
				       false, NULL, 0, 0, 0, srcline);
}

static int thread__resolve_callchain_unwind(struct thread *thread,
					    struct callchain_cursor *cursor,
					    struct evsel *evsel,
					    struct perf_sample *sample,
					    int max_stack)
{
	 
	if (!((evsel->core.attr.sample_type & PERF_SAMPLE_REGS_USER) &&
	      (evsel->core.attr.sample_type & PERF_SAMPLE_STACK_USER)))
		return 0;

	 
	if ((!sample->user_regs.regs) ||
	    (!sample->user_stack.size))
		return 0;

	return unwind__get_entries(unwind_entry, cursor,
				   thread, sample, max_stack, false);
}

int thread__resolve_callchain(struct thread *thread,
			      struct callchain_cursor *cursor,
			      struct evsel *evsel,
			      struct perf_sample *sample,
			      struct symbol **parent,
			      struct addr_location *root_al,
			      int max_stack)
{
	int ret = 0;

	if (cursor == NULL)
		return -ENOMEM;

	callchain_cursor_reset(cursor);

	if (callchain_param.order == ORDER_CALLEE) {
		ret = thread__resolve_callchain_sample(thread, cursor,
						       evsel, sample,
						       parent, root_al,
						       max_stack);
		if (ret)
			return ret;
		ret = thread__resolve_callchain_unwind(thread, cursor,
						       evsel, sample,
						       max_stack);
	} else {
		ret = thread__resolve_callchain_unwind(thread, cursor,
						       evsel, sample,
						       max_stack);
		if (ret)
			return ret;
		ret = thread__resolve_callchain_sample(thread, cursor,
						       evsel, sample,
						       parent, root_al,
						       max_stack);
	}

	return ret;
}

int machine__for_each_thread(struct machine *machine,
			     int (*fn)(struct thread *thread, void *p),
			     void *priv)
{
	struct threads *threads;
	struct rb_node *nd;
	int rc = 0;
	int i;

	for (i = 0; i < THREADS__TABLE_SIZE; i++) {
		threads = &machine->threads[i];
		for (nd = rb_first_cached(&threads->entries); nd;
		     nd = rb_next(nd)) {
			struct thread_rb_node *trb = rb_entry(nd, struct thread_rb_node, rb_node);

			rc = fn(trb->thread, priv);
			if (rc != 0)
				return rc;
		}
	}
	return rc;
}

int machines__for_each_thread(struct machines *machines,
			      int (*fn)(struct thread *thread, void *p),
			      void *priv)
{
	struct rb_node *nd;
	int rc = 0;

	rc = machine__for_each_thread(&machines->host, fn, priv);
	if (rc != 0)
		return rc;

	for (nd = rb_first_cached(&machines->guests); nd; nd = rb_next(nd)) {
		struct machine *machine = rb_entry(nd, struct machine, rb_node);

		rc = machine__for_each_thread(machine, fn, priv);
		if (rc != 0)
			return rc;
	}
	return rc;
}

pid_t machine__get_current_tid(struct machine *machine, int cpu)
{
	if (cpu < 0 || (size_t)cpu >= machine->current_tid_sz)
		return -1;

	return machine->current_tid[cpu];
}

int machine__set_current_tid(struct machine *machine, int cpu, pid_t pid,
			     pid_t tid)
{
	struct thread *thread;
	const pid_t init_val = -1;

	if (cpu < 0)
		return -EINVAL;

	if (realloc_array_as_needed(machine->current_tid,
				    machine->current_tid_sz,
				    (unsigned int)cpu,
				    &init_val))
		return -ENOMEM;

	machine->current_tid[cpu] = tid;

	thread = machine__findnew_thread(machine, pid, tid);
	if (!thread)
		return -ENOMEM;

	thread__set_cpu(thread, cpu);
	thread__put(thread);

	return 0;
}

 
bool machine__is(struct machine *machine, const char *arch)
{
	return machine && !strcmp(perf_env__raw_arch(machine->env), arch);
}

bool machine__normalized_is(struct machine *machine, const char *arch)
{
	return machine && !strcmp(perf_env__arch(machine->env), arch);
}

int machine__nr_cpus_avail(struct machine *machine)
{
	return machine ? perf_env__nr_cpus_avail(machine->env) : 0;
}

int machine__get_kernel_start(struct machine *machine)
{
	struct map *map = machine__kernel_map(machine);
	int err = 0;

	 
	machine->kernel_start = 1ULL << 63;
	if (map) {
		err = map__load(map);
		 
		if (!err && !machine__is(machine, "x86_64"))
			machine->kernel_start = map__start(map);
	}
	return err;
}

u8 machine__addr_cpumode(struct machine *machine, u8 cpumode, u64 addr)
{
	u8 addr_cpumode = cpumode;
	bool kernel_ip;

	if (!machine->single_address_space)
		goto out;

	kernel_ip = machine__kernel_ip(machine, addr);
	switch (cpumode) {
	case PERF_RECORD_MISC_KERNEL:
	case PERF_RECORD_MISC_USER:
		addr_cpumode = kernel_ip ? PERF_RECORD_MISC_KERNEL :
					   PERF_RECORD_MISC_USER;
		break;
	case PERF_RECORD_MISC_GUEST_KERNEL:
	case PERF_RECORD_MISC_GUEST_USER:
		addr_cpumode = kernel_ip ? PERF_RECORD_MISC_GUEST_KERNEL :
					   PERF_RECORD_MISC_GUEST_USER;
		break;
	default:
		break;
	}
out:
	return addr_cpumode;
}

struct dso *machine__findnew_dso_id(struct machine *machine, const char *filename, struct dso_id *id)
{
	return dsos__findnew_id(&machine->dsos, filename, id);
}

struct dso *machine__findnew_dso(struct machine *machine, const char *filename)
{
	return machine__findnew_dso_id(machine, filename, NULL);
}

char *machine__resolve_kernel_addr(void *vmachine, unsigned long long *addrp, char **modp)
{
	struct machine *machine = vmachine;
	struct map *map;
	struct symbol *sym = machine__find_kernel_symbol(machine, *addrp, &map);

	if (sym == NULL)
		return NULL;

	*modp = __map__is_kmodule(map) ? (char *)map__dso(map)->short_name : NULL;
	*addrp = map__unmap_ip(map, sym->start);
	return sym->name;
}

int machine__for_each_dso(struct machine *machine, machine__dso_t fn, void *priv)
{
	struct dso *pos;
	int err = 0;

	list_for_each_entry(pos, &machine->dsos.head, node) {
		if (fn(pos, machine, priv))
			err = -1;
	}
	return err;
}

int machine__for_each_kernel_map(struct machine *machine, machine__map_t fn, void *priv)
{
	struct maps *maps = machine__kernel_maps(machine);
	struct map_rb_node *pos;
	int err = 0;

	maps__for_each_entry(maps, pos) {
		err = fn(pos->map, priv);
		if (err != 0) {
			break;
		}
	}
	return err;
}

bool machine__is_lock_function(struct machine *machine, u64 addr)
{
	if (!machine->sched.text_start) {
		struct map *kmap;
		struct symbol *sym = machine__find_kernel_symbol_by_name(machine, "__sched_text_start", &kmap);

		if (!sym) {
			 
			machine->sched.text_start = 1;
			return false;
		}

		machine->sched.text_start = map__unmap_ip(kmap, sym->start);

		 
		sym = machine__find_kernel_symbol_by_name(machine, "__sched_text_end", &kmap);
		machine->sched.text_end = map__unmap_ip(kmap, sym->start);

		sym = machine__find_kernel_symbol_by_name(machine, "__lock_text_start", &kmap);
		machine->lock.text_start = map__unmap_ip(kmap, sym->start);

		sym = machine__find_kernel_symbol_by_name(machine, "__lock_text_end", &kmap);
		machine->lock.text_end = map__unmap_ip(kmap, sym->start);
	}

	 
	if (machine->sched.text_start == 1)
		return false;

	 
	if (machine->sched.text_start <= addr && addr < machine->sched.text_end)
		return true;

	 
	if (machine->lock.text_start <= addr && addr < machine->lock.text_end)
		return true;

	return false;
}
