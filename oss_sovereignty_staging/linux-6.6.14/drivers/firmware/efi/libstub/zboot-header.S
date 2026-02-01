 

#include <linux/pe.h>

#ifdef CONFIG_64BIT
	.set		.Lextra_characteristics, 0x0
	.set		.Lpe_opt_magic, PE_OPT_MAGIC_PE32PLUS
#else
	.set		.Lextra_characteristics, IMAGE_FILE_32BIT_MACHINE
	.set		.Lpe_opt_magic, PE_OPT_MAGIC_PE32
#endif

	.section	".head", "a"
	.globl		__efistub_efi_zboot_header
__efistub_efi_zboot_header:
.Ldoshdr:
	.long		MZ_MAGIC
	.ascii		"zimg"					 
	.long		__efistub__gzdata_start - .Ldoshdr	 
	.long		__efistub__gzdata_size - ZBOOT_SIZE_LEN	 
	.long		0, 0					 
	.asciz		COMP_TYPE				 
	.org		.Ldoshdr + 0x38
	.long		LINUX_PE_MAGIC
	.long		.Lpehdr - .Ldoshdr			 

.Lpehdr:
	.long		PE_MAGIC
	.short		MACHINE_TYPE
	.short		.Lsection_count
	.long		0
	.long		0
	.long		0
	.short		.Lsection_table - .Loptional_header
	.short		IMAGE_FILE_DEBUG_STRIPPED | \
			IMAGE_FILE_EXECUTABLE_IMAGE | \
			IMAGE_FILE_LINE_NUMS_STRIPPED |\
			.Lextra_characteristics

.Loptional_header:
	.short		.Lpe_opt_magic
	.byte		0, 0
	.long		_etext - .Lefi_header_end
	.long		__data_size
	.long		0
	.long		__efistub_efi_zboot_entry - .Ldoshdr
	.long		.Lefi_header_end - .Ldoshdr

#ifdef CONFIG_64BIT
	.quad		0
#else
	.long		_etext - .Ldoshdr, 0x0
#endif
	.long		4096
	.long		512
	.short		0, 0
	.short		LINUX_EFISTUB_MAJOR_VERSION	 
	.short		LINUX_EFISTUB_MINOR_VERSION	 
	.short		0, 0
	.long		0
	.long		_end - .Ldoshdr

	.long		.Lefi_header_end - .Ldoshdr
	.long		0
	.short		IMAGE_SUBSYSTEM_EFI_APPLICATION
	.short		IMAGE_DLL_CHARACTERISTICS_NX_COMPAT
#ifdef CONFIG_64BIT
	.quad		0, 0, 0, 0
#else
	.long		0, 0, 0, 0
#endif
	.long		0
	.long		(.Lsection_table - .) / 8

	.quad		0				 
	.quad		0				 
	.quad		0				 
	.quad		0				 
	.quad		0				 
	.quad		0				 
#if defined(PE_DLL_CHAR_EX) || defined(CONFIG_DEBUG_EFI)
	.long		.Lefi_debug_table - .Ldoshdr	 
	.long		.Lefi_debug_table_size

	.section	".rodata", "a"
	.p2align	2
.Lefi_debug_table:
	 
#ifdef PE_DLL_CHAR_EX
	.long		0					 
	.long		0					 
	.short		0					 
	.short		0					 
	.long		IMAGE_DEBUG_TYPE_EX_DLLCHARACTERISTICS	 
	.long		4					 
	.long		0					 
	.long		.Lefi_dll_characteristics_ex - .Ldoshdr	 
#endif
#ifdef CONFIG_DEBUG_EFI
	.long		0					 
	.long		0					 
	.short		0					 
	.short		0					 
	.long		IMAGE_DEBUG_TYPE_CODEVIEW		 
	.long		.Lefi_debug_entry_size			 
	.long		0					 
	.long		.Lefi_debug_entry - .Ldoshdr		 
#endif
	.set		.Lefi_debug_table_size, . - .Lefi_debug_table
	.previous
#endif

.Lsection_table:
	.ascii		".text\0\0\0"
	.long		_etext - .Lefi_header_end
	.long		.Lefi_header_end - .Ldoshdr
	.long		_etext - .Lefi_header_end
	.long		.Lefi_header_end - .Ldoshdr

	.long		0, 0
	.short		0, 0
	.long		IMAGE_SCN_CNT_CODE | \
			IMAGE_SCN_MEM_READ | \
			IMAGE_SCN_MEM_EXECUTE

	.ascii		".data\0\0\0"
	.long		__data_size
	.long		_etext - .Ldoshdr
	.long		__data_rawsize
	.long		_etext - .Ldoshdr

	.long		0, 0
	.short		0, 0
	.long		IMAGE_SCN_CNT_INITIALIZED_DATA | \
			IMAGE_SCN_MEM_READ | \
			IMAGE_SCN_MEM_WRITE

	.set		.Lsection_count, (. - .Lsection_table) / 40

#ifdef PE_DLL_CHAR_EX
.Lefi_dll_characteristics_ex:
	.long		PE_DLL_CHAR_EX
#endif
#ifdef CONFIG_DEBUG_EFI
.Lefi_debug_entry:
	 
	.ascii		"NB10"				 
	.long		0				 
	.long		0				 
	.long		0				 

	.asciz		ZBOOT_EFI_PATH

	.set		.Lefi_debug_entry_size, . - .Lefi_debug_entry
#endif

	.p2align	12
.Lefi_header_end:

