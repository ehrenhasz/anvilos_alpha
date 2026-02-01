
 

#include "dso.h"
#include "symbol.h"
#include "map.h"
#include "probe-event.h"
#include "probe-file.h"

int arch__choose_best_symbol(struct symbol *syma,
			     struct symbol *symb __maybe_unused)
{
	char *sym = syma->name;

#if !defined(_CALL_ELF) || _CALL_ELF != 2
	 
	if (*sym == '.')
		sym++;
#endif

	 
	if (strlen(sym) >= 3 && !strncmp(sym, "SyS", 3))
		return SYMBOL_B;
	if (strlen(sym) >= 10 && !strncmp(sym, "compat_SyS", 10))
		return SYMBOL_B;

	return SYMBOL_A;
}

#if !defined(_CALL_ELF) || _CALL_ELF != 2
 
int arch__compare_symbol_names(const char *namea, const char *nameb)
{
	 
	if (*namea == '.')
		namea++;
	if (*nameb == '.')
		nameb++;

	return strcmp(namea, nameb);
}

int arch__compare_symbol_names_n(const char *namea, const char *nameb,
				 unsigned int n)
{
	 
	if (*namea == '.')
		namea++;
	if (*nameb == '.')
		nameb++;

	return strncmp(namea, nameb, n);
}

const char *arch__normalize_symbol_name(const char *name)
{
	 
	if (name && *name == '.')
		name++;
	return name;
}
#endif

#if defined(_CALL_ELF) && _CALL_ELF == 2

#ifdef HAVE_LIBELF_SUPPORT
void arch__sym_update(struct symbol *s, GElf_Sym *sym)
{
	s->arch_sym = sym->st_other;
}
#endif

#define PPC64LE_LEP_OFFSET	8

void arch__fix_tev_from_maps(struct perf_probe_event *pev,
			     struct probe_trace_event *tev, struct map *map,
			     struct symbol *sym)
{
	int lep_offset;

	 
	if (pev->point.offset || !map || !sym)
		return;

	 
	if (!pev->uprobes && pev->point.retprobe) {
#ifdef HAVE_LIBELF_SUPPORT
		if (!kretprobe_offset_is_supported())
#endif
			return;
	}

	lep_offset = PPC64_LOCAL_ENTRY_OFFSET(sym->arch_sym);

	if (map__dso(map)->symtab_type == DSO_BINARY_TYPE__KALLSYMS)
		tev->point.offset += PPC64LE_LEP_OFFSET;
	else if (lep_offset) {
		if (pev->uprobes)
			tev->point.address += lep_offset;
		else
			tev->point.offset += lep_offset;
	}
}

#ifdef HAVE_LIBELF_SUPPORT
void arch__post_process_probe_trace_events(struct perf_probe_event *pev,
					   int ntevs)
{
	struct probe_trace_event *tev;
	struct map *map;
	struct symbol *sym = NULL;
	struct rb_node *tmp;
	int i = 0;

	map = get_target_map(pev->target, pev->nsi, pev->uprobes);
	if (!map || map__load(map) < 0)
		return;

	for (i = 0; i < ntevs; i++) {
		tev = &pev->tevs[i];
		map__for_each_symbol(map, sym, tmp) {
			if (map__unmap_ip(map, sym->start) == tev->point.address) {
				arch__fix_tev_from_maps(pev, tev, map, sym);
				break;
			}
		}
	}
}
#endif  

#endif
