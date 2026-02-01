
#include <linux/slab.h>  
#include <linux/consolemap.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/device.h>  
#include <linux/selection.h>
#include <linux/workqueue.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/atomic.h>
#include <linux/console.h>

#include "speakup.h"

unsigned short spk_xs, spk_ys, spk_xe, spk_ye;  
struct vc_data *spk_sel_cons;

struct speakup_selection_work {
	struct work_struct work;
	struct tiocl_selection sel;
	struct tty_struct *tty;
};

static void __speakup_set_selection(struct work_struct *work)
{
	struct speakup_selection_work *ssw =
		container_of(work, struct speakup_selection_work, work);

	struct tty_struct *tty;
	struct tiocl_selection sel;

	sel = ssw->sel;

	 
	rmb();

	 
	tty = xchg(&ssw->tty, NULL);

	if (spk_sel_cons != vc_cons[fg_console].d) {
		spk_sel_cons = vc_cons[fg_console].d;
		pr_warn("Selection: mark console not the same as cut\n");
		goto unref;
	}

	console_lock();
	clear_selection();
	console_unlock();

	set_selection_kernel(&sel, tty);

unref:
	tty_kref_put(tty);
}

static struct speakup_selection_work speakup_sel_work = {
	.work = __WORK_INITIALIZER(speakup_sel_work.work,
				   __speakup_set_selection)
};

int speakup_set_selection(struct tty_struct *tty)
{
	 
	tty_kref_get(tty);
	if (cmpxchg(&speakup_sel_work.tty, NULL, tty)) {
		tty_kref_put(tty);
		return -EBUSY;
	}
	 
	wmb();

	speakup_sel_work.sel.xs = spk_xs + 1;
	speakup_sel_work.sel.ys = spk_ys + 1;
	speakup_sel_work.sel.xe = spk_xe + 1;
	speakup_sel_work.sel.ye = spk_ye + 1;
	speakup_sel_work.sel.sel_mode = TIOCL_SELCHAR;

	schedule_work_on(WORK_CPU_UNBOUND, &speakup_sel_work.work);

	return 0;
}

void speakup_cancel_selection(void)
{
	struct tty_struct *tty;

	cancel_work_sync(&speakup_sel_work.work);
	 
	tty = xchg(&speakup_sel_work.tty, NULL);
	if (tty)
		tty_kref_put(tty);
}

static void __speakup_paste_selection(struct work_struct *work)
{
	struct speakup_selection_work *ssw =
		container_of(work, struct speakup_selection_work, work);
	struct tty_struct *tty = xchg(&ssw->tty, NULL);

	paste_selection(tty);
	tty_kref_put(tty);
}

static struct speakup_selection_work speakup_paste_work = {
	.work = __WORK_INITIALIZER(speakup_paste_work.work,
				   __speakup_paste_selection)
};

int speakup_paste_selection(struct tty_struct *tty)
{
	tty_kref_get(tty);
	if (cmpxchg(&speakup_paste_work.tty, NULL, tty)) {
		tty_kref_put(tty);
		return -EBUSY;
	}

	schedule_work_on(WORK_CPU_UNBOUND, &speakup_paste_work.work);
	return 0;
}

void speakup_cancel_paste(void)
{
	struct tty_struct *tty;

	cancel_work_sync(&speakup_paste_work.work);
	tty = xchg(&speakup_paste_work.tty, NULL);
	if (tty)
		tty_kref_put(tty);
}
