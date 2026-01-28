

#include <linux/linkage.h>
#include <asm/page_types.h>

	.section ".init.data","aw"

	.balign PAGE_SIZE

SYM_DATA_START(real_mode_blob)
	.incbin	"arch/x86/realmode/rm/realmode.bin"
SYM_DATA_END_LABEL(real_mode_blob, SYM_L_GLOBAL, real_mode_blob_end)

SYM_DATA_START(real_mode_relocs)
	.incbin	"arch/x86/realmode/rm/realmode.relocs"
SYM_DATA_END(real_mode_relocs)
