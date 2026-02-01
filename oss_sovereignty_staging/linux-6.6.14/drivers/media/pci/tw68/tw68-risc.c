
 

#include "tw68.h"

 
static __le32 *tw68_risc_field(__le32 *rp, struct scatterlist *sglist,
			    unsigned int offset, u32 sync_line,
			    unsigned int bpl, unsigned int padding,
			    unsigned int lines, bool jump)
{
	struct scatterlist *sg;
	unsigned int line, todo, done;

	if (jump) {
		*(rp++) = cpu_to_le32(RISC_JUMP);
		*(rp++) = 0;
	}

	 
	if (sync_line == 1)
		*(rp++) = cpu_to_le32(RISC_SYNCO);
	else
		*(rp++) = cpu_to_le32(RISC_SYNCE);
	*(rp++) = 0;

	 
	sg = sglist;
	for (line = 0; line < lines; line++) {
		 
		while (offset && offset >= sg_dma_len(sg)) {
			offset -= sg_dma_len(sg);
			sg = sg_next(sg);
		}
		if (bpl <= sg_dma_len(sg) - offset) {
			 
			*(rp++) = cpu_to_le32(RISC_LINESTART |
					         bpl);
			*(rp++) = cpu_to_le32(sg_dma_address(sg) + offset);
			offset += bpl;
		} else {
			 
			todo = bpl;	 
			 
			done = (sg_dma_len(sg) - offset);
			*(rp++) = cpu_to_le32(RISC_LINESTART |
						(7 << 24) |
						done);
			*(rp++) = cpu_to_le32(sg_dma_address(sg) + offset);
			todo -= done;
			sg = sg_next(sg);
			 
			while (todo > sg_dma_len(sg)) {
				*(rp++) = cpu_to_le32(RISC_INLINE |
						(done << 12) |
						sg_dma_len(sg));
				*(rp++) = cpu_to_le32(sg_dma_address(sg));
				todo -= sg_dma_len(sg);
				sg = sg_next(sg);
				done += sg_dma_len(sg);
			}
			if (todo) {
				 
				*(rp++) = cpu_to_le32(RISC_INLINE |
							(done << 12) |
							todo);
				*(rp++) = cpu_to_le32(sg_dma_address(sg));
			}
			offset = todo;
		}
		offset += padding;
	}

	return rp;
}

 
int tw68_risc_buffer(struct pci_dev *pci,
			struct tw68_buf *buf,
			struct scatterlist *sglist,
			unsigned int top_offset,
			unsigned int bottom_offset,
			unsigned int bpl,
			unsigned int padding,
			unsigned int lines)
{
	u32 instructions, fields;
	__le32 *rp;

	fields = 0;
	if (UNSET != top_offset)
		fields++;
	if (UNSET != bottom_offset)
		fields++;
	 
	instructions  = fields * (1 + (((bpl + padding) * lines) /
			 PAGE_SIZE) + lines) + 4;
	buf->size = instructions * 8;
	buf->cpu = dma_alloc_coherent(&pci->dev, buf->size, &buf->dma,
				      GFP_KERNEL);
	if (buf->cpu == NULL)
		return -ENOMEM;

	 
	rp = buf->cpu;
	if (UNSET != top_offset)	 
		rp = tw68_risc_field(rp, sglist, top_offset, 1,
				     bpl, padding, lines, true);
	if (UNSET != bottom_offset)	 
		rp = tw68_risc_field(rp, sglist, bottom_offset, 2,
				     bpl, padding, lines, top_offset == UNSET);

	 
	buf->jmp = rp;
	buf->cpu[1] = cpu_to_le32(buf->dma + 8);
	 
	BUG_ON((buf->jmp - buf->cpu + 2) * sizeof(buf->cpu[0]) > buf->size);
	return 0;
}

#if 0
 
 

static void tw68_risc_decode(u32 risc, u32 addr)
{
#define	RISC_OP(reg)	(((reg) >> 28) & 7)
	static struct instr_details {
		char *name;
		u8 has_data_type;
		u8 has_byte_info;
		u8 has_addr;
	} instr[8] = {
		[RISC_OP(RISC_SYNCO)]	  = {"syncOdd", 0, 0, 0},
		[RISC_OP(RISC_SYNCE)]	  = {"syncEven", 0, 0, 0},
		[RISC_OP(RISC_JUMP)]	  = {"jump", 0, 0, 1},
		[RISC_OP(RISC_LINESTART)] = {"lineStart", 1, 1, 1},
		[RISC_OP(RISC_INLINE)]	  = {"inline", 1, 1, 1},
	};
	u32 p;

	p = RISC_OP(risc);
	if (!(risc & 0x80000000) || !instr[p].name) {
		pr_debug("0x%08x [ INVALID ]\n", risc);
		return;
	}
	pr_debug("0x%08x %-9s IRQ=%d",
		risc, instr[p].name, (risc >> 27) & 1);
	if (instr[p].has_data_type)
		pr_debug(" Type=%d", (risc >> 24) & 7);
	if (instr[p].has_byte_info)
		pr_debug(" Start=0x%03x Count=%03u",
			(risc >> 12) & 0xfff, risc & 0xfff);
	if (instr[p].has_addr)
		pr_debug(" StartAddr=0x%08x", addr);
	pr_debug("\n");
}

void tw68_risc_program_dump(struct tw68_core *core, struct tw68_buf *buf)
{
	const __le32 *addr;

	pr_debug("%s: risc_program_dump: risc=%p, buf->cpu=0x%p, buf->jmp=0x%p\n",
		  core->name, buf, buf->cpu, buf->jmp);
	for (addr = buf->cpu; addr <= buf->jmp; addr += 2)
		tw68_risc_decode(*addr, *(addr+1));
}
#endif
