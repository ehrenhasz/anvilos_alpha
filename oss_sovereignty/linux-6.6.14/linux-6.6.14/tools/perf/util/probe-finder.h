#ifndef _PROBE_FINDER_H
#define _PROBE_FINDER_H
#include <stdbool.h>
#include "intlist.h"
#include "build-id.h"
#include "probe-event.h"
#include <linux/ctype.h>
#define MAX_PROBE_BUFFER	1024
#define MAX_PROBES		 128
#define MAX_PROBE_ARGS		 128
#define PROBE_ARG_VARS		"$vars"
#define PROBE_ARG_PARAMS	"$params"
static inline int is_c_varname(const char *name)
{
	return isalpha(name[0]) || name[0] == '_';
}
#ifdef HAVE_DWARF_SUPPORT
#include "dwarf-aux.h"
struct debuginfo {
	Dwarf		*dbg;
	Dwfl_Module	*mod;
	Dwfl		*dwfl;
	Dwarf_Addr	bias;
	const unsigned char	*build_id;
};
struct debuginfo *debuginfo__new(const char *path);
void debuginfo__delete(struct debuginfo *dbg);
int debuginfo__find_trace_events(struct debuginfo *dbg,
				 struct perf_probe_event *pev,
				 struct probe_trace_event **tevs);
int debuginfo__find_probe_point(struct debuginfo *dbg, u64 addr,
				struct perf_probe_point *ppt);
int debuginfo__get_text_offset(struct debuginfo *dbg, Dwarf_Addr *offs,
			       bool adjust_offset);
int debuginfo__find_line_range(struct debuginfo *dbg, struct line_range *lr);
int debuginfo__find_available_vars_at(struct debuginfo *dbg,
				      struct perf_probe_event *pev,
				      struct variable_list **vls);
int find_source_path(const char *raw_path, const char *sbuild_id,
		     const char *comp_dir, char **new_path);
struct probe_finder {
	struct perf_probe_event	*pev;		 
	struct debuginfo	*dbg;
	int (*callback)(Dwarf_Die *sc_die, struct probe_finder *pf);
	int			lno;		 
	Dwarf_Addr		addr;		 
	const char		*fname;		 
	Dwarf_Die		cu_die;		 
	Dwarf_Die		sp_die;
	struct intlist		*lcache;	 
#if _ELFUTILS_PREREQ(0, 142)
	Dwarf_CFI		*cfi_eh;
	Dwarf_CFI		*cfi_dbg;
#endif
	Dwarf_Op		*fb_ops;	 
	unsigned int		machine;	 
	struct perf_probe_arg	*pvar;		 
	struct probe_trace_arg	*tvar;		 
	bool			skip_empty_arg;	 
};
struct trace_event_finder {
	struct probe_finder	pf;
	Dwfl_Module		*mod;		 
	struct probe_trace_event *tevs;		 
	int			ntevs;		 
	int			max_tevs;	 
};
struct available_var_finder {
	struct probe_finder	pf;
	Dwfl_Module		*mod;		 
	struct variable_list	*vls;		 
	int			nvls;		 
	int			max_vls;	 
	bool			child;		 
};
struct line_finder {
	struct line_range	*lr;		 
	const char		*fname;		 
	int			lno_s;		 
	int			lno_e;		 
	Dwarf_Die		cu_die;		 
	Dwarf_Die		sp_die;
	int			found;
};
#endif  
#endif  
