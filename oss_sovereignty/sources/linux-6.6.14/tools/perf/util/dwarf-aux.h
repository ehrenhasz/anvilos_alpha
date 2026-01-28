
#ifndef _DWARF_AUX_H
#define _DWARF_AUX_H


#include <dwarf.h>
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <elfutils/version.h>

struct strbuf;


const char *cu_find_realpath(Dwarf_Die *cu_die, const char *fname);


const char *cu_get_comp_dir(Dwarf_Die *cu_die);


int cu_find_lineinfo(Dwarf_Die *cudie, Dwarf_Addr addr,
		     const char **fname, int *lineno);


int cu_walk_functions_at(Dwarf_Die *cu_die, Dwarf_Addr addr,
			 int (*callback)(Dwarf_Die *, void *), void *data);


const char *die_get_linkage_name(Dwarf_Die *dw_die);


int die_entrypc(Dwarf_Die *dw_die, Dwarf_Addr *addr);


bool die_is_func_def(Dwarf_Die *dw_die);


bool die_is_func_instance(Dwarf_Die *dw_die);


bool die_compare_name(Dwarf_Die *dw_die, const char *tname);


bool die_match_name(Dwarf_Die *dw_die, const char *glob);


int die_get_call_lineno(Dwarf_Die *in_die);


const char *die_get_call_file(Dwarf_Die *in_die);


const char *die_get_decl_file(Dwarf_Die *dw_die);


Dwarf_Die *die_get_type(Dwarf_Die *vr_die, Dwarf_Die *die_mem);


Dwarf_Die *die_get_real_type(Dwarf_Die *vr_die, Dwarf_Die *die_mem);


bool die_is_signed_type(Dwarf_Die *tp_die);


int die_get_data_member_location(Dwarf_Die *mb_die, Dwarf_Word *offs);


enum {
	DIE_FIND_CB_END = 0,		
	DIE_FIND_CB_CHILD = 1,		
	DIE_FIND_CB_SIBLING = 2,	
	DIE_FIND_CB_CONTINUE = 3,	
};


Dwarf_Die *die_find_child(Dwarf_Die *rt_die,
			 int (*callback)(Dwarf_Die *, void *),
			 void *data, Dwarf_Die *die_mem);


Dwarf_Die *die_find_realfunc(Dwarf_Die *cu_die, Dwarf_Addr addr,
			     Dwarf_Die *die_mem);


Dwarf_Die *die_find_tailfunc(Dwarf_Die *cu_die, Dwarf_Addr addr,
				    Dwarf_Die *die_mem);


Dwarf_Die *die_find_top_inlinefunc(Dwarf_Die *sp_die, Dwarf_Addr addr,
				   Dwarf_Die *die_mem);


Dwarf_Die *die_find_inlinefunc(Dwarf_Die *sp_die, Dwarf_Addr addr,
			       Dwarf_Die *die_mem);


int die_walk_instances(Dwarf_Die *in_die,
		       int (*callback)(Dwarf_Die *, void *), void *data);


typedef int (* line_walk_callback_t) (const char *fname, int lineno,
				      Dwarf_Addr addr, void *data);


int die_walk_lines(Dwarf_Die *rt_die, line_walk_callback_t callback, void *data);


Dwarf_Die *die_find_variable_at(Dwarf_Die *sp_die, const char *name,
				Dwarf_Addr addr, Dwarf_Die *die_mem);


Dwarf_Die *die_find_member(Dwarf_Die *st_die, const char *name,
			   Dwarf_Die *die_mem);


int die_get_typename(Dwarf_Die *vr_die, struct strbuf *buf);


int die_get_varname(Dwarf_Die *vr_die, struct strbuf *buf);
int die_get_var_range(Dwarf_Die *sp_die, Dwarf_Die *vr_die, struct strbuf *buf);


bool die_is_optimized_target(Dwarf_Die *cu_die);


void die_skip_prologue(Dwarf_Die *sp_die, Dwarf_Die *cu_die,
		       Dwarf_Addr *entrypc);

#endif
