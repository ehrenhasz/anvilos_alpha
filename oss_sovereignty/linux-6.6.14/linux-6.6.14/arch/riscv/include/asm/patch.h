#ifndef _ASM_RISCV_PATCH_H
#define _ASM_RISCV_PATCH_H
int patch_text_nosync(void *addr, const void *insns, size_t len);
int patch_text_set_nosync(void *addr, u8 c, size_t len);
int patch_text(void *addr, u32 *insns, int ninsns);
extern int riscv_patch_in_stop_machine;
#endif  
