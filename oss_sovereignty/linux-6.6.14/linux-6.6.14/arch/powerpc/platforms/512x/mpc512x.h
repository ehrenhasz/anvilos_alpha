#ifndef __MPC512X_H__
#define __MPC512X_H__
extern void __init mpc512x_init_IRQ(void);
extern void __init mpc512x_init_early(void);
extern void __init mpc512x_init(void);
extern void __init mpc512x_setup_arch(void);
extern int __init mpc5121_clk_init(void);
const char *__init mpc512x_select_psc_compat(void);
extern void __noreturn mpc512x_restart(char *cmd);
#endif				 
