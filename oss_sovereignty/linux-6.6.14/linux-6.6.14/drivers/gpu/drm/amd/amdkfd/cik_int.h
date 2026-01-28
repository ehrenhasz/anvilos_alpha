#ifndef CIK_INT_H_INCLUDED
#define CIK_INT_H_INCLUDED
#include <linux/types.h>
struct cik_ih_ring_entry {
	uint32_t source_id;
	uint32_t data;
	uint32_t ring_id;
	uint32_t reserved;
};
#define CIK_INTSRC_CP_END_OF_PIPE	0xB5
#define CIK_INTSRC_CP_BAD_OPCODE	0xB7
#define CIK_INTSRC_SDMA_TRAP		0xE0
#define CIK_INTSRC_SQ_INTERRUPT_MSG	0xEF
#define CIK_INTSRC_GFX_PAGE_INV_FAULT	0x92
#define CIK_INTSRC_GFX_MEM_PROT_FAULT	0x93
#endif
