


#ifdef CONFIG_X86_32
#define LOAD_OFFSET __PAGE_OFFSET
#else
#define LOAD_OFFSET __START_KERNEL_map
#endif

#define RUNTIME_DISCARD_EXIT
#define EMITS_PT_NOTE
#define RO_EXCEPTION_TABLE_ALIGN	16

#include <asm-generic/vmlinux.lds.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
#include <asm/page_types.h>
#include <asm/orc_lookup.h>
#include <asm/cache.h>
#include <asm/boot.h>

#undef i386     

OUTPUT_FORMAT(CONFIG_OUTPUT_FORMAT)

#ifdef CONFIG_X86_32
OUTPUT_ARCH(i386)
ENTRY(phys_startup_32)
#else
OUTPUT_ARCH(i386:x86-64)
ENTRY(phys_startup_64)
#endif

jiffies = jiffies_64;

#if defined(CONFIG_X86_64)

#define X86_ALIGN_RODATA_BEGIN	. = ALIGN(HPAGE_SIZE);

#define X86_ALIGN_RODATA_END					\
		. = ALIGN(HPAGE_SIZE);				\
		__end_rodata_hpage_align = .;			\
		__end_rodata_aligned = .;

#define ALIGN_ENTRY_TEXT_BEGIN	. = ALIGN(PMD_SIZE);
#define ALIGN_ENTRY_TEXT_END	. = ALIGN(PMD_SIZE);


#define BSS_DECRYPTED						\
	. = ALIGN(PMD_SIZE);					\
	__start_bss_decrypted = .;				\
	*(.bss..decrypted);					\
	. = ALIGN(PAGE_SIZE);					\
	__start_bss_decrypted_unused = .;			\
	. = ALIGN(PMD_SIZE);					\
	__end_bss_decrypted = .;				\

#else

#define X86_ALIGN_RODATA_BEGIN
#define X86_ALIGN_RODATA_END					\
		. = ALIGN(PAGE_SIZE);				\
		__end_rodata_aligned = .;

#define ALIGN_ENTRY_TEXT_BEGIN
#define ALIGN_ENTRY_TEXT_END
#define BSS_DECRYPTED

#endif

PHDRS {
	text PT_LOAD FLAGS(5);          
	data PT_LOAD FLAGS(6);          
#ifdef CONFIG_X86_64
#ifdef CONFIG_SMP
	percpu PT_LOAD FLAGS(6);        
#endif
	init PT_LOAD FLAGS(7);          
#endif
	note PT_NOTE FLAGS(0);          
}

SECTIONS
{
#ifdef CONFIG_X86_32
	. = LOAD_OFFSET + LOAD_PHYSICAL_ADDR;
	phys_startup_32 = ABSOLUTE(startup_32 - LOAD_OFFSET);
#else
	. = __START_KERNEL;
	phys_startup_64 = ABSOLUTE(startup_64 - LOAD_OFFSET);
#endif

	
	.text :  AT(ADDR(.text) - LOAD_OFFSET) {
		_text = .;
		_stext = .;
		
		HEAD_TEXT
		TEXT_TEXT
		SCHED_TEXT
		LOCK_TEXT
		KPROBES_TEXT
		SOFTIRQENTRY_TEXT
#ifdef CONFIG_RETPOLINE
		*(.text..__x86.indirect_thunk)
		*(.text..__x86.return_thunk)
#endif
		STATIC_CALL_TEXT

		ALIGN_ENTRY_TEXT_BEGIN
#ifdef CONFIG_CPU_SRSO
		*(.text..__x86.rethunk_untrain)
#endif

		ENTRY_TEXT

#ifdef CONFIG_CPU_SRSO
		
		. = srso_alias_untrain_ret | (1 << 2) | (1 << 8) | (1 << 14) | (1 << 20);
		*(.text..__x86.rethunk_safe)
#endif
		ALIGN_ENTRY_TEXT_END
		*(.gnu.warning)

	} :text = 0xcccccccc

	
	_etext = .;
	. = ALIGN(PAGE_SIZE);

	X86_ALIGN_RODATA_BEGIN
	RO_DATA(PAGE_SIZE)
	X86_ALIGN_RODATA_END

	
	.data : AT(ADDR(.data) - LOAD_OFFSET) {
		
		_sdata = .;

		
		INIT_TASK_DATA(THREAD_SIZE)

#ifdef CONFIG_X86_32
		
		NOSAVE_DATA
#endif

		PAGE_ALIGNED_DATA(PAGE_SIZE)

		CACHELINE_ALIGNED_DATA(L1_CACHE_BYTES)

		DATA_DATA
		CONSTRUCTORS

		
		READ_MOSTLY_DATA(INTERNODE_CACHE_BYTES)

		
		_edata = .;
	} :data

	BUG_TABLE

	ORC_UNWIND_TABLE

	. = ALIGN(PAGE_SIZE);
	__vvar_page = .;

	.vvar : AT(ADDR(.vvar) - LOAD_OFFSET) {
		
		__vvar_beginning_hack = .;

		
#define EMIT_VVAR(name, offset)				\
		. = __vvar_beginning_hack + offset;	\
		*(.vvar_ ## name)
#include <asm/vvar.h>
#undef EMIT_VVAR

		
		. = __vvar_beginning_hack + PAGE_SIZE;
	} :data

	. = ALIGN(__vvar_page + PAGE_SIZE, PAGE_SIZE);

	
	. = ALIGN(PAGE_SIZE);
	.init.begin : AT(ADDR(.init.begin) - LOAD_OFFSET) {
		__init_begin = .; 
	}

#if defined(CONFIG_X86_64) && defined(CONFIG_SMP)
	
	PERCPU_VADDR(INTERNODE_CACHE_BYTES, 0, :percpu)
	ASSERT(SIZEOF(.data..percpu) < CONFIG_PHYSICAL_START,
	       "per-CPU data too large - increase CONFIG_PHYSICAL_START")
#endif

	INIT_TEXT_SECTION(PAGE_SIZE)
#ifdef CONFIG_X86_64
	:init
#endif

	
	.altinstr_aux : AT(ADDR(.altinstr_aux) - LOAD_OFFSET) {
		*(.altinstr_aux)
	}

	INIT_DATA_SECTION(16)

	.x86_cpu_dev.init : AT(ADDR(.x86_cpu_dev.init) - LOAD_OFFSET) {
		__x86_cpu_dev_start = .;
		*(.x86_cpu_dev.init)
		__x86_cpu_dev_end = .;
	}

#ifdef CONFIG_X86_INTEL_MID
	.x86_intel_mid_dev.init : AT(ADDR(.x86_intel_mid_dev.init) - \
								LOAD_OFFSET) {
		__x86_intel_mid_dev_start = .;
		*(.x86_intel_mid_dev.init)
		__x86_intel_mid_dev_end = .;
	}
#endif

	
	. = ALIGN(8);
	.parainstructions : AT(ADDR(.parainstructions) - LOAD_OFFSET) {
		__parainstructions = .;
		*(.parainstructions)
		__parainstructions_end = .;
	}

#ifdef CONFIG_RETPOLINE
	
	. = ALIGN(8);
	.retpoline_sites : AT(ADDR(.retpoline_sites) - LOAD_OFFSET) {
		__retpoline_sites = .;
		*(.retpoline_sites)
		__retpoline_sites_end = .;
	}

	. = ALIGN(8);
	.return_sites : AT(ADDR(.return_sites) - LOAD_OFFSET) {
		__return_sites = .;
		*(.return_sites)
		__return_sites_end = .;
	}

	. = ALIGN(8);
	.call_sites : AT(ADDR(.call_sites) - LOAD_OFFSET) {
		__call_sites = .;
		*(.call_sites)
		__call_sites_end = .;
	}
#endif

#ifdef CONFIG_X86_KERNEL_IBT
	. = ALIGN(8);
	.ibt_endbr_seal : AT(ADDR(.ibt_endbr_seal) - LOAD_OFFSET) {
		__ibt_endbr_seal = .;
		*(.ibt_endbr_seal)
		__ibt_endbr_seal_end = .;
	}
#endif

#ifdef CONFIG_FINEIBT
	. = ALIGN(8);
	.cfi_sites : AT(ADDR(.cfi_sites) - LOAD_OFFSET) {
		__cfi_sites = .;
		*(.cfi_sites)
		__cfi_sites_end = .;
	}
#endif

	
	. = ALIGN(8);
	.altinstructions : AT(ADDR(.altinstructions) - LOAD_OFFSET) {
		__alt_instructions = .;
		*(.altinstructions)
		__alt_instructions_end = .;
	}

	
	.altinstr_replacement : AT(ADDR(.altinstr_replacement) - LOAD_OFFSET) {
		*(.altinstr_replacement)
	}

	. = ALIGN(8);
	.apicdrivers : AT(ADDR(.apicdrivers) - LOAD_OFFSET) {
		__apicdrivers = .;
		*(.apicdrivers);
		__apicdrivers_end = .;
	}

	. = ALIGN(8);
	
	.exit.text : AT(ADDR(.exit.text) - LOAD_OFFSET) {
		EXIT_TEXT
	}

	.exit.data : AT(ADDR(.exit.data) - LOAD_OFFSET) {
		EXIT_DATA
	}

#if !defined(CONFIG_X86_64) || !defined(CONFIG_SMP)
	PERCPU_SECTION(INTERNODE_CACHE_BYTES)
#endif

	. = ALIGN(PAGE_SIZE);

	
	.init.end : AT(ADDR(.init.end) - LOAD_OFFSET) {
		__init_end = .;
	}

	
	. = ALIGN(PAGE_SIZE);
	.smp_locks : AT(ADDR(.smp_locks) - LOAD_OFFSET) {
		__smp_locks = .;
		*(.smp_locks)
		. = ALIGN(PAGE_SIZE);
		__smp_locks_end = .;
	}

#ifdef CONFIG_X86_64
	.data_nosave : AT(ADDR(.data_nosave) - LOAD_OFFSET) {
		NOSAVE_DATA
	}
#endif

	
	. = ALIGN(PAGE_SIZE);
	.bss : AT(ADDR(.bss) - LOAD_OFFSET) {
		__bss_start = .;
		*(.bss..page_aligned)
		. = ALIGN(PAGE_SIZE);
		*(BSS_MAIN)
		BSS_DECRYPTED
		. = ALIGN(PAGE_SIZE);
		__bss_stop = .;
	}

	
	__end_of_kernel_reserve = .;

	. = ALIGN(PAGE_SIZE);
	.brk : AT(ADDR(.brk) - LOAD_OFFSET) {
		__brk_base = .;
		. += 64 * 1024;		
		*(.bss..brk)		
		__brk_limit = .;
	}

	. = ALIGN(PAGE_SIZE);		
	_end = .;

#ifdef CONFIG_AMD_MEM_ENCRYPT
	
	. = ALIGN(HPAGE_SIZE);
	.init.scratch : AT(ADDR(.init.scratch) - LOAD_OFFSET) {
		__init_scratch_begin = .;
		*(.init.scratch)
		. = ALIGN(HPAGE_SIZE);
		__init_scratch_end = .;
	}
#endif

	STABS_DEBUG
	DWARF_DEBUG
	ELF_DETAILS

	DISCARDS

	
	.got.plt (INFO) : { *(.got.plt) }
	ASSERT(SIZEOF(.got.plt) == 0 ||
#ifdef CONFIG_X86_64
	       SIZEOF(.got.plt) == 0x18,
#else
	       SIZEOF(.got.plt) == 0xc,
#endif
	       "Unexpected GOT/PLT entries detected!")

	
	.got : {
		*(.got) *(.igot.*)
	}
	ASSERT(SIZEOF(.got) == 0, "Unexpected GOT entries detected!")

	.plt : {
		*(.plt) *(.plt.*) *(.iplt)
	}
	ASSERT(SIZEOF(.plt) == 0, "Unexpected run-time procedure linkages detected!")

	.rel.dyn : {
		*(.rel.*) *(.rel_*)
	}
	ASSERT(SIZEOF(.rel.dyn) == 0, "Unexpected run-time relocations (.rel) detected!")

	.rela.dyn : {
		*(.rela.*) *(.rela_*)
	}
	ASSERT(SIZEOF(.rela.dyn) == 0, "Unexpected run-time relocations (.rela) detected!")
}


. = ASSERT((_end - LOAD_OFFSET <= KERNEL_IMAGE_SIZE),
	   "kernel image bigger than KERNEL_IMAGE_SIZE");

#ifdef CONFIG_X86_64

#define INIT_PER_CPU(x) init_per_cpu__##x = ABSOLUTE(x) + __per_cpu_load
INIT_PER_CPU(gdt_page);
INIT_PER_CPU(fixed_percpu_data);
INIT_PER_CPU(irq_stack_backing_store);

#ifdef CONFIG_SMP
. = ASSERT((fixed_percpu_data == 0),
           "fixed_percpu_data is not at start of per-cpu area");
#endif

#ifdef CONFIG_RETHUNK
. = ASSERT((retbleed_return_thunk & 0x3f) == 0, "retbleed_return_thunk not cacheline-aligned");
. = ASSERT((srso_safe_ret & 0x3f) == 0, "srso_safe_ret not cacheline-aligned");
#endif

#ifdef CONFIG_CPU_SRSO

. = ASSERT(((ABSOLUTE(srso_alias_untrain_ret) | srso_alias_safe_ret) -
		(ABSOLUTE(srso_alias_untrain_ret) & srso_alias_safe_ret)) == ((1 << 2) | (1 << 8) | (1 << 14) | (1 << 20)),
		"SRSO function pair won't alias");
#endif

#endif 
