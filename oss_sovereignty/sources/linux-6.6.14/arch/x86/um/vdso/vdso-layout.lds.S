


SECTIONS
{
	. = VDSO_PRELINK + SIZEOF_HEADERS;

	.hash		: { *(.hash) }			:text
	.gnu.hash	: { *(.gnu.hash) }
	.dynsym		: { *(.dynsym) }
	.dynstr		: { *(.dynstr) }
	.gnu.version	: { *(.gnu.version) }
	.gnu.version_d	: { *(.gnu.version_d) }
	.gnu.version_r	: { *(.gnu.version_r) }

	.note		: { *(.note.*) }		:text	:note

	.eh_frame_hdr	: { *(.eh_frame_hdr) }		:text	:eh_frame_hdr
	.eh_frame	: { KEEP (*(.eh_frame)) }	:text

	.dynamic	: { *(.dynamic) }		:text	:dynamic

	.rodata		: { *(.rodata*) }		:text
	.data		: {
	      *(.data*)
	      *(.sdata*)
	      *(.got.plt) *(.got)
	      *(.gnu.linkonce.d.*)
	      *(.bss*)
	      *(.dynbss*)
	      *(.gnu.linkonce.b.*)
	}

	.altinstructions	: { *(.altinstructions) }
	.altinstr_replacement	: { *(.altinstr_replacement) }

	
	. = ALIGN(0x100);

	.text		: { *(.text*) }			:text	=0x90909090
}


#define PT_GNU_EH_FRAME	0x6474e550


PHDRS
{
	text		PT_LOAD		FLAGS(5) FILEHDR PHDRS; 
	dynamic		PT_DYNAMIC	FLAGS(4);		
	note		PT_NOTE		FLAGS(4);		
	eh_frame_hdr	PT_GNU_EH_FRAME;
}
