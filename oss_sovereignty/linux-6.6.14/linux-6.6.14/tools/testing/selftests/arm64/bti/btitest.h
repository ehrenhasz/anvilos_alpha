#ifndef BTITEST_H
#define BTITEST_H
void call_using_br_x0(void (*)(void));
void call_using_br_x16(void (*)(void));
void call_using_blr(void (*)(void));
void nohint_func(void);
void bti_none_func(void);
void bti_c_func(void);
void bti_j_func(void);
void bti_jc_func(void);
void paciasp_func(void);
#endif  
