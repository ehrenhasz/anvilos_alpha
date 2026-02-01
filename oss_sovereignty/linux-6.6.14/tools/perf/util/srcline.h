 
#ifndef PERF_SRCLINE_H
#define PERF_SRCLINE_H

#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/types.h>

struct dso;
struct symbol;

extern int addr2line_timeout_ms;
extern bool srcline_full_filename;
char *get_srcline(struct dso *dso, u64 addr, struct symbol *sym,
		  bool show_sym, bool show_addr, u64 ip);
char *__get_srcline(struct dso *dso, u64 addr, struct symbol *sym,
		  bool show_sym, bool show_addr, bool unwind_inlines,
		  u64 ip);
void zfree_srcline(char **srcline);
char *get_srcline_split(struct dso *dso, u64 addr, unsigned *line);

 
void srcline__tree_insert(struct rb_root_cached *tree, u64 addr, char *srcline);
 
char *srcline__tree_find(struct rb_root_cached *tree, u64 addr);
 
void srcline__tree_delete(struct rb_root_cached *tree);

extern char *srcline__unknown;
#define SRCLINE_UNKNOWN srcline__unknown

struct inline_list {
	struct symbol		*symbol;
	char			*srcline;
	struct list_head	list;
};

struct inline_node {
	u64			addr;
	struct list_head	val;
	struct rb_node		rb_node;
};

 
struct inline_node *dso__parse_addr_inlines(struct dso *dso, u64 addr,
					    struct symbol *sym);
 
void inline_node__delete(struct inline_node *node);

 
void inlines__tree_insert(struct rb_root_cached *tree,
			  struct inline_node *inlines);
 
struct inline_node *inlines__tree_find(struct rb_root_cached *tree, u64 addr);
 
void inlines__tree_delete(struct rb_root_cached *tree);

#endif  
