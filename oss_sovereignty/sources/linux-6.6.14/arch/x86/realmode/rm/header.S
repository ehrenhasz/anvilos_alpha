


#include <linux/linkage.h>
#include <asm/page_types.h>
#include <asm/segment.h>

#include "realmode.h"
	
	.section ".header", "a"

	.balign	16
SYM_DATA_START(real_mode_header)
	.long	pa_text_start
	.long	pa_ro_end
	
	.long	pa_trampoline_start
	.long	pa_trampoline_header
#ifdef CONFIG_AMD_MEM_ENCRYPT
	.long	pa_sev_es_trampoline_start
#endif
#ifdef CONFIG_X86_64
	.long	pa_trampoline_start64
	.long	pa_trampoline_pgd;
#endif
	
#ifdef CONFIG_ACPI_SLEEP
	.long	pa_wakeup_start
	.long	pa_wakeup_header
#endif
	
	.long	pa_machine_real_restart_asm
#ifdef CONFIG_X86_64
	.long	__KERNEL32_CS
#endif
SYM_DATA_END(real_mode_header)

	
	.section ".signature","a"
	.balign 4
SYM_DATA(end_signature, .long REALMODE_END_SIGNATURE)
