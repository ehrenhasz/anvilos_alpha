#ifndef __ASM_EXTABLE_H
#define __ASM_EXTABLE_H
struct exception_table_entry {
        unsigned int insn, fixup;
};
#endif
