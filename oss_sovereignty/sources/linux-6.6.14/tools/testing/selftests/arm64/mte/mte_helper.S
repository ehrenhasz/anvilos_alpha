


#include "mte_def.h"

.arch	armv8.5-a+memtag

#define ENTRY(name) \
	.globl name ;\
	.p2align 2;\
	.type name, @function ;\
name:

#define ENDPROC(name) \
	.size name, .-name ;

	.text

ENTRY(mte_insert_random_tag)
	irg	x0, x0, xzr
	ret
ENDPROC(mte_insert_random_tag)


ENTRY(mte_insert_new_tag)
	gmi	x1, x0, xzr
	irg	x0, x0, x1
	ret
ENDPROC(mte_insert_new_tag)


ENTRY(mte_get_tag_address)
	ldg	x0, [x0]
	ret
ENDPROC(mte_get_tag_address)


ENTRY(mte_set_tag_address_range)
	cbz	x1, 2f
1:
	stg	x0, [x0, #0x0]
	add	x0, x0, #MT_GRANULE_SIZE
	sub	x1, x1, #MT_GRANULE_SIZE
	cbnz	x1, 1b
2:
	ret
ENDPROC(mte_set_tag_address_range)


ENTRY(mte_clear_tag_address_range)
	cbz	x1, 2f
1:
	stzg	x0, [x0, #0x0]
	add	x0, x0, #MT_GRANULE_SIZE
	sub	x1, x1, #MT_GRANULE_SIZE
	cbnz	x1, 1b
2:
	ret
ENDPROC(mte_clear_tag_address_range)


ENTRY(mte_enable_pstate_tco)
	msr	tco, #MT_PSTATE_TCO_EN
	ret
ENDPROC(mte_enable_pstate_tco)


ENTRY(mte_disable_pstate_tco)
	msr	tco, #MT_PSTATE_TCO_DIS
	ret
ENDPROC(mte_disable_pstate_tco)


ENTRY(mte_get_pstate_tco)
	mrs	x0, tco
	ubfx	x0, x0, #MT_PSTATE_TCO_SHIFT, #1
	ret
ENDPROC(mte_get_pstate_tco)
