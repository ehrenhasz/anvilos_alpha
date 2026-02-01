
#include "debug.h"
#include "dsos.h"
#include "dso.h"
#include "util.h"
#include "vdso.h"
#include "namespaces.h"
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <symbol.h> 
#include <unistd.h>

static int __dso_id__cmp(struct dso_id *a, struct dso_id *b)
{
	if (a->maj > b->maj) return -1;
	if (a->maj < b->maj) return 1;

	if (a->min > b->min) return -1;
	if (a->min < b->min) return 1;

	if (a->ino > b->ino) return -1;
	if (a->ino < b->ino) return 1;

	 
	if (a->ino_generation && b->ino_generation) {
		if (a->ino_generation > b->ino_generation) return -1;
		if (a->ino_generation < b->ino_generation) return 1;
	}

	return 0;
}

static bool dso_id__empty(struct dso_id *id)
{
	if (!id)
		return true;

	return !id->maj && !id->min && !id->ino && !id->ino_generation;
}

static void dso__inject_id(struct dso *dso, struct dso_id *id)
{
	dso->id.maj = id->maj;
	dso->id.min = id->min;
	dso->id.ino = id->ino;
	dso->id.ino_generation = id->ino_generation;
}

static int dso_id__cmp(struct dso_id *a, struct dso_id *b)
{
	 
	if (dso_id__empty(a) || dso_id__empty(b))
		return 0;

	return __dso_id__cmp(a, b);
}

int dso__cmp_id(struct dso *a, struct dso *b)
{
	return __dso_id__cmp(&a->id, &b->id);
}

bool __dsos__read_build_ids(struct list_head *head, bool with_hits)
{
	bool have_build_id = false;
	struct dso *pos;
	struct nscookie nsc;

	list_for_each_entry(pos, head, node) {
		if (with_hits && !pos->hit && !dso__is_vdso(pos))
			continue;
		if (pos->has_build_id) {
			have_build_id = true;
			continue;
		}
		nsinfo__mountns_enter(pos->nsinfo, &nsc);
		if (filename__read_build_id(pos->long_name, &pos->bid) > 0) {
			have_build_id	  = true;
			pos->has_build_id = true;
		} else if (errno == ENOENT && pos->nsinfo) {
			char *new_name = dso__filename_with_chroot(pos, pos->long_name);

			if (new_name && filename__read_build_id(new_name,
								&pos->bid) > 0) {
				have_build_id = true;
				pos->has_build_id = true;
			}
			free(new_name);
		}
		nsinfo__mountns_exit(&nsc);
	}

	return have_build_id;
}

static int __dso__cmp_long_name(const char *long_name, struct dso_id *id, struct dso *b)
{
	int rc = strcmp(long_name, b->long_name);
	return rc ?: dso_id__cmp(id, &b->id);
}

static int __dso__cmp_short_name(const char *short_name, struct dso_id *id, struct dso *b)
{
	int rc = strcmp(short_name, b->short_name);
	return rc ?: dso_id__cmp(id, &b->id);
}

static int dso__cmp_short_name(struct dso *a, struct dso *b)
{
	return __dso__cmp_short_name(a->short_name, &a->id, b);
}

 
struct dso *__dsos__findnew_link_by_longname_id(struct rb_root *root, struct dso *dso,
						const char *name, struct dso_id *id)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node  *parent = NULL;

	if (!name)
		name = dso->long_name;
	 
	while (*p) {
		struct dso *this = rb_entry(*p, struct dso, rb_node);
		int rc = __dso__cmp_long_name(name, id, this);

		parent = *p;
		if (rc == 0) {
			 
			if (!dso || (dso == this))
				return this;	 
			 
			rc = dso__cmp_short_name(dso, this);
			if (rc == 0) {
				pr_err("Duplicated dso name: %s\n", name);
				return NULL;
			}
		}
		if (rc < 0)
			p = &parent->rb_left;
		else
			p = &parent->rb_right;
	}
	if (dso) {
		 
		rb_link_node(&dso->rb_node, parent, p);
		rb_insert_color(&dso->rb_node, root);
		dso->root = root;
	}
	return NULL;
}

void __dsos__add(struct dsos *dsos, struct dso *dso)
{
	list_add_tail(&dso->node, &dsos->head);
	__dsos__findnew_link_by_longname_id(&dsos->root, dso, NULL, &dso->id);
	 
	dso__get(dso);
}

void dsos__add(struct dsos *dsos, struct dso *dso)
{
	down_write(&dsos->lock);
	__dsos__add(dsos, dso);
	up_write(&dsos->lock);
}

static struct dso *__dsos__findnew_by_longname_id(struct rb_root *root, const char *name, struct dso_id *id)
{
	return __dsos__findnew_link_by_longname_id(root, NULL, name, id);
}

static struct dso *__dsos__find_id(struct dsos *dsos, const char *name, struct dso_id *id, bool cmp_short)
{
	struct dso *pos;

	if (cmp_short) {
		list_for_each_entry(pos, &dsos->head, node)
			if (__dso__cmp_short_name(name, id, pos) == 0)
				return pos;
		return NULL;
	}
	return __dsos__findnew_by_longname_id(&dsos->root, name, id);
}

struct dso *__dsos__find(struct dsos *dsos, const char *name, bool cmp_short)
{
	return __dsos__find_id(dsos, name, NULL, cmp_short);
}

static void dso__set_basename(struct dso *dso)
{
	char *base, *lname;
	int tid;

	if (sscanf(dso->long_name, "/tmp/perf-%d.map", &tid) == 1) {
		if (asprintf(&base, "[JIT] tid %d", tid) < 0)
			return;
	} else {
	       
		lname = strdup(dso->long_name);
		if (!lname)
			return;

		 
		base = strdup(basename(lname));

		free(lname);

		if (!base)
			return;
	}
	dso__set_short_name(dso, base, true);
}

static struct dso *__dsos__addnew_id(struct dsos *dsos, const char *name, struct dso_id *id)
{
	struct dso *dso = dso__new_id(name, id);

	if (dso != NULL) {
		__dsos__add(dsos, dso);
		dso__set_basename(dso);
		 
		dso__put(dso);
	}
	return dso;
}

struct dso *__dsos__addnew(struct dsos *dsos, const char *name)
{
	return __dsos__addnew_id(dsos, name, NULL);
}

static struct dso *__dsos__findnew_id(struct dsos *dsos, const char *name, struct dso_id *id)
{
	struct dso *dso = __dsos__find_id(dsos, name, id, false);

	if (dso && dso_id__empty(&dso->id) && !dso_id__empty(id))
		dso__inject_id(dso, id);

	return dso ? dso : __dsos__addnew_id(dsos, name, id);
}

struct dso *dsos__findnew_id(struct dsos *dsos, const char *name, struct dso_id *id)
{
	struct dso *dso;
	down_write(&dsos->lock);
	dso = dso__get(__dsos__findnew_id(dsos, name, id));
	up_write(&dsos->lock);
	return dso;
}

size_t __dsos__fprintf_buildid(struct list_head *head, FILE *fp,
			       bool (skip)(struct dso *dso, int parm), int parm)
{
	struct dso *pos;
	size_t ret = 0;

	list_for_each_entry(pos, head, node) {
		char sbuild_id[SBUILD_ID_SIZE];

		if (skip && skip(pos, parm))
			continue;
		build_id__sprintf(&pos->bid, sbuild_id);
		ret += fprintf(fp, "%-40s %s\n", sbuild_id, pos->long_name);
	}
	return ret;
}

size_t __dsos__fprintf(struct list_head *head, FILE *fp)
{
	struct dso *pos;
	size_t ret = 0;

	list_for_each_entry(pos, head, node) {
		ret += dso__fprintf(pos, fp);
	}

	return ret;
}
