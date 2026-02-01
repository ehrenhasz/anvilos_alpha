  

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/panic_notifier.h>
#include <linux/reboot.h>
#include <linux/sched/signal.h>
#include <linux/kthread.h>
#include <linux/pm.h>

#include <asm/pdc.h>
#include <asm/io.h>
#include <asm/led.h>

#define DRIVER_NAME  "powersw"
#define KTHREAD_NAME "kpowerswd"

 
#define POWERSWITCH_POLL_PER_SEC 2

 
#define POWERSWITCH_DOWN_SEC 2

 
 
#define DIAG_CODE(code)		(0x14000000 + ((code)<<5))

#define MFCPU_X(rDiagReg, t_ch, t_th, code) \
	(DIAG_CODE(code) + ((rDiagReg)<<21) + ((t_ch)<<16) + ((t_th)<<0) )
	
#define MTCPU(dr, gr)		MFCPU_X(dr, gr,  0, 0x12)        
#define MFCPU_C(dr, gr)		MFCPU_X(dr, gr,  0, 0x30)	 
#define MFCPU_T(dr, gr)		MFCPU_X(dr,  0, gr, 0xa0)	 
	
#define __getDIAG(dr) ( { 			\
        register unsigned long __res asm("r28");\
	 __asm__ __volatile__ (			\
		".word %1" : "=&r" (__res) : "i" (MFCPU_T(dr,28) ) \
	);					\
	__res;					\
} )

 
static int shutdown_timer __read_mostly;

 
static void process_shutdown(void)
{
	if (shutdown_timer == 0)
		printk(KERN_ALERT KTHREAD_NAME ": Shutdown requested...\n");

	shutdown_timer++;
	
	 
	if (shutdown_timer == (POWERSWITCH_DOWN_SEC*POWERSWITCH_POLL_PER_SEC)) {
		static const char msg[] = "Shutting down...";
		printk(KERN_INFO KTHREAD_NAME ": %s\n", msg);
		lcd_print(msg);

		 
		if (kill_cad_pid(SIGINT, 1)) {
			 
			machine_power_off();
		}
	}
}


 
static struct task_struct *power_task;

 
#define SYSCTL_FILENAME	"sys/kernel/power"

 
int pwrsw_enabled __read_mostly = 1;

 
static int kpowerswd(void *param)
{
	__set_current_state(TASK_RUNNING);

	do {
		int button_not_pressed;
		unsigned long soft_power_reg = (unsigned long) param;

		schedule_timeout_interruptible(pwrsw_enabled ? HZ : HZ/POWERSWITCH_POLL_PER_SEC);

		if (unlikely(!pwrsw_enabled))
			continue;

		if (soft_power_reg) {
			 
			button_not_pressed = (gsc_readl(soft_power_reg) & 0x1);
		} else {
			 
			button_not_pressed = (__getDIAG(25) & 0x80000000);
		}

		if (likely(button_not_pressed)) {
			if (unlikely(shutdown_timer &&  
				shutdown_timer < (POWERSWITCH_DOWN_SEC*POWERSWITCH_POLL_PER_SEC))) {
				shutdown_timer = 0;
				printk(KERN_INFO KTHREAD_NAME ": Shutdown request aborted.\n");
			}
		} else
			process_shutdown();


	} while (!kthread_should_stop());

	return 0;
}


 
#if 0
static void powerfail_interrupt(int code, void *x)
{
	printk(KERN_CRIT "POWERFAIL INTERRUPTION !\n");
	poweroff();
}
#endif




 
static int parisc_panic_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	 
	pdc_soft_power_button_panic(0);
	return NOTIFY_DONE;
}

static struct notifier_block parisc_panic_block = {
	.notifier_call	= parisc_panic_event,
	.priority	= INT_MAX,
};

 
static int qemu_power_off(struct sys_off_data *data)
{
	 
	gsc_writel(0, (unsigned long) data->cb_data);
	pdc_soft_power_button(1);
	return NOTIFY_DONE;
}

static int __init power_init(void)
{
	unsigned long ret;
	unsigned long soft_power_reg;

#if 0
	request_irq( IRQ_FROM_REGION(CPU_IRQ_REGION)+2, &powerfail_interrupt,
		0, "powerfail", NULL);
#endif

	 
	ret = pdc_soft_power_info(&soft_power_reg);
	if (ret == PDC_OK)
		ret = pdc_soft_power_button(1);
	if (ret != PDC_OK)
		soft_power_reg = -1UL;
	
	switch (soft_power_reg) {
	case 0:		printk(KERN_INFO DRIVER_NAME ": Gecko-style soft power switch enabled.\n");
			break;
			
	case -1UL:	printk(KERN_INFO DRIVER_NAME ": Soft power switch support not available.\n");
			return -ENODEV;
	
	default:	printk(KERN_INFO DRIVER_NAME ": Soft power switch at 0x%08lx enabled.\n",
				soft_power_reg);
	}

	power_task = NULL;
	if (running_on_qemu && soft_power_reg)
		register_sys_off_handler(SYS_OFF_MODE_POWER_OFF, SYS_OFF_PRIO_DEFAULT,
					qemu_power_off, (void *)soft_power_reg);
	else
		power_task = kthread_run(kpowerswd, (void*)soft_power_reg,
					KTHREAD_NAME);
	if (IS_ERR(power_task)) {
		printk(KERN_ERR DRIVER_NAME ": thread creation failed.  Driver not loaded.\n");
		pdc_soft_power_button(0);
		return -EIO;
	}

	 
	atomic_notifier_chain_register(&panic_notifier_list,
			&parisc_panic_block);

	return 0;
}

static void __exit power_exit(void)
{
	kthread_stop(power_task);

	atomic_notifier_chain_unregister(&panic_notifier_list,
			&parisc_panic_block);

	pdc_soft_power_button(0);
}

arch_initcall(power_init);
module_exit(power_exit);


MODULE_AUTHOR("Helge Deller <deller@gmx.de>");
MODULE_DESCRIPTION("Soft power switch driver");
MODULE_LICENSE("Dual BSD/GPL");
