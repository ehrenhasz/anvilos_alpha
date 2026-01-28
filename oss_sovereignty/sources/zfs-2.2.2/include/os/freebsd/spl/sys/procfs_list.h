


#ifndef	_SPL_PROCFS_LIST_H
#define	_SPL_PROCFS_LIST_H

#ifndef _STANDALONE

#include <sys/kstat.h>
#include <sys/mutex.h>




typedef struct procfs_list procfs_list_t;
struct procfs_list {
	void		*pl_private;
	void		*pl_next_data;
	kmutex_t	pl_lock;
	list_t		pl_list;
	uint64_t	pl_next_id;
	int		(*pl_show)(struct seq_file *f, void *p);
	int		(*pl_show_header)(struct seq_file *f);
	int		(*pl_clear)(procfs_list_t *procfs_list);
	size_t		pl_node_offset;
};

typedef struct procfs_list_node {
	list_node_t	pln_link;
	uint64_t	pln_id;
} procfs_list_node_t;

void procfs_list_install(const char *module,
    const char *submodule,
    const char *name,
    mode_t mode,
    procfs_list_t *procfs_list,
    int (*show)(struct seq_file *f, void *p),
    int (*show_header)(struct seq_file *f),
    int (*clear)(procfs_list_t *procfs_list),
    size_t procfs_list_node_off);
void procfs_list_uninstall(procfs_list_t *procfs_list);
void procfs_list_destroy(procfs_list_t *procfs_list);
void procfs_list_add(procfs_list_t *procfs_list, void *p);

#else
typedef int procfs_list_t;
#endif 

#endif	
