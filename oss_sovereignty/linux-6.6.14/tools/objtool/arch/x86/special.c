
#include <string.h>

#include <objtool/special.h>
#include <objtool/builtin.h>

#define X86_FEATURE_POPCNT (4 * 32 + 23)
#define X86_FEATURE_SMAP   (9 * 32 + 20)

void arch_handle_alternative(unsigned short feature, struct special_alt *alt)
{
	switch (feature) {
	case X86_FEATURE_SMAP:
		 
		if (opts.uaccess)
			alt->skip_orig = true;
		else
			alt->skip_alt = true;
		break;
	case X86_FEATURE_POPCNT:
		 
		alt->skip_orig = true;
		break;
	default:
		break;
	}
}

bool arch_support_alt_relocation(struct special_alt *special_alt,
				 struct instruction *insn,
				 struct reloc *reloc)
{
	return true;
}

 
struct reloc *arch_find_switch_table(struct objtool_file *file,
				    struct instruction *insn)
{
	struct reloc  *text_reloc, *rodata_reloc;
	struct section *table_sec;
	unsigned long table_offset;

	 
	text_reloc = find_reloc_by_dest_range(file->elf, insn->sec,
					      insn->offset, insn->len);
	if (!text_reloc || text_reloc->sym->type != STT_SECTION ||
	    !text_reloc->sym->sec->rodata)
		return NULL;

	table_offset = reloc_addend(text_reloc);
	table_sec = text_reloc->sym->sec;

	if (reloc_type(text_reloc) == R_X86_64_PC32)
		table_offset += 4;

	 
	if (find_symbol_containing(table_sec, table_offset) &&
	    strcmp(table_sec->name, C_JUMP_TABLE_SECTION))
		return NULL;

	 
	rodata_reloc = find_reloc_by_dest(file->elf, table_sec, table_offset);
	if (!rodata_reloc)
		return NULL;

	 
	if (reloc_type(text_reloc) == R_X86_64_PC32)
		file->ignore_unreachables = true;

	return rodata_reloc;
}
