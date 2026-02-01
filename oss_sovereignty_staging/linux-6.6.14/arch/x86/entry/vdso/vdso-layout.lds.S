 
#include <asm/vdso.h>

 

SECTIONS
{
	 

	vvar_start = . - 4 * PAGE_SIZE;
	vvar_page  = vvar_start;

	 
#define EMIT_VVAR(name, offset) vvar_ ## name = vvar_page + offset;
#include <asm/vvar.h>
#undef EMIT_VVAR

	pvclock_page = vvar_start + PAGE_SIZE;
	hvclock_page = vvar_start + 2 * PAGE_SIZE;
	timens_page  = vvar_start + 3 * PAGE_SIZE;

#undef _ASM_X86_VVAR_H
	 
#define EMIT_VVAR(name, offset) timens_ ## name = timens_page + offset;
#include <asm/vvar.h>
#undef EMIT_VVAR

	. = SIZEOF_HEADERS;

	.hash		: { *(.hash) }			:text
	.gnu.hash	: { *(.gnu.hash) }
	.dynsym		: { *(.dynsym) }
	.dynstr		: { *(.dynstr) }
	.gnu.version	: { *(.gnu.version) }
	.gnu.version_d	: { *(.gnu.version_d) }
	.gnu.version_r	: { *(.gnu.version_r) }

	.dynamic	: { *(.dynamic) }		:text	:dynamic

	.rodata		: {
		*(.rodata*)
		*(.data*)
		*(.sdata*)
		*(.got.plt) *(.got)
		*(.gnu.linkonce.d.*)
		*(.bss*)
		*(.dynbss*)
		*(.gnu.linkonce.b.*)
	}						:text

	 
	/DISCARD/ : {
		*(.note.gnu.property)
	}
	.note		: { *(.note.*) }		:text	:note

	.eh_frame_hdr	: { *(.eh_frame_hdr) }		:text	:eh_frame_hdr
	.eh_frame	: { KEEP (*(.eh_frame)) }	:text


	 

	.text		: {
		*(.text*)
	}						:text	=0x90909090,



	.altinstructions	: { *(.altinstructions) }	:text
	.altinstr_replacement	: { *(.altinstr_replacement) }	:text

	__ex_table		: { *(__ex_table) }		:text

	/DISCARD/ : {
		*(.discard)
		*(.discard.*)
		*(__bug_table)
	}
}

 
#define PT_GNU_EH_FRAME	0x6474e550

 
PHDRS
{
	text		PT_LOAD		FLAGS(5) FILEHDR PHDRS;  
	dynamic		PT_DYNAMIC	FLAGS(4);		 
	note		PT_NOTE		FLAGS(4);		 
	eh_frame_hdr	PT_GNU_EH_FRAME;
}
