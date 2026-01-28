#ifndef __DC_FPU_H__
#define __DC_FPU_H__
void dc_assert_fp_enabled(void);
void dc_fpu_begin(const char *function_name, const int line);
void dc_fpu_end(const char *function_name, const int line);
#endif  
