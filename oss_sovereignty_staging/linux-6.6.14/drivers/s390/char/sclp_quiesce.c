
 

#include <linux/types.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/atomic.h>
#include <asm/ptrace.h>
#include <asm/smp.h>

#include "sclp.h"

 
static void do_machine_quiesce(void)
{
	psw_t quiesce_psw;

	smp_send_stop();
	quiesce_psw.mask =
		PSW_MASK_BASE | PSW_MASK_EA | PSW_MASK_BA | PSW_MASK_WAIT;
	quiesce_psw.addr = 0xfff;
	__load_psw(quiesce_psw);
}

 
static void sclp_quiesce_handler(struct evbuf_header *evbuf)
{
	_machine_restart = (void *) do_machine_quiesce;
	_machine_halt = do_machine_quiesce;
	_machine_power_off = do_machine_quiesce;
	ctrl_alt_del();
}

static struct sclp_register sclp_quiesce_event = {
	.receive_mask = EVTYP_SIGQUIESCE_MASK,
	.receiver_fn = sclp_quiesce_handler,
};

 
static int __init sclp_quiesce_init(void)
{
	return sclp_register(&sclp_quiesce_event);
}
device_initcall(sclp_quiesce_init);
