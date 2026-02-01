
 

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/uaccess.h>

#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/consolemap.h>
#include <linux/selection.h>
#include <linux/tiocl.h>
#include <linux/console.h>
#include <linux/tty_flip.h>

#include <linux/sched/signal.h>

 
#define is_space_on_vt(c)	((c) == ' ')

 
static struct vc_selection {
	struct mutex lock;
	struct vc_data *cons;			 
	char *buffer;
	unsigned int buf_len;
	volatile int start;			 
	int end;
} vc_sel = {
	.lock = __MUTEX_INITIALIZER(vc_sel.lock),
	.start = -1,
};

 

 
static inline void highlight(const int s, const int e)
{
	invert_screen(vc_sel.cons, s, e-s+2, true);
}

 
static inline void highlight_pointer(const int where)
{
	complement_pos(vc_sel.cons, where);
}

static u32
sel_pos(int n, bool unicode)
{
	if (unicode)
		return screen_glyph_unicode(vc_sel.cons, n / 2);
	return inverse_translate(vc_sel.cons, screen_glyph(vc_sel.cons, n),
			false);
}

 
void clear_selection(void)
{
	highlight_pointer(-1);  
	if (vc_sel.start != -1) {
		highlight(vc_sel.start, vc_sel.end);
		vc_sel.start = -1;
	}
}
EXPORT_SYMBOL_GPL(clear_selection);

bool vc_is_sel(struct vc_data *vc)
{
	return vc == vc_sel.cons;
}

 
static u32 inwordLut[]={
  0x00000000,  
  0x03FFE000,  
  0x87FFFFFE,  
  0x07FFFFFE,  
};

static inline int inword(const u32 c)
{
	return c > 0x7f || (( inwordLut[c>>5] >> (c & 0x1F) ) & 1);
}

 
int sel_loadlut(char __user *p)
{
	u32 tmplut[ARRAY_SIZE(inwordLut)];
	if (copy_from_user(tmplut, (u32 __user *)(p+4), sizeof(inwordLut)))
		return -EFAULT;
	memcpy(inwordLut, tmplut, sizeof(inwordLut));
	return 0;
}

 
static inline int atedge(const int p, int size_row)
{
	return (!(p % size_row)	|| !((p + 2) % size_row));
}

 
static int store_utf8(u32 c, char *p)
{
	if (c < 0x80) {
		 
		p[0] = c;
		return 1;
	} else if (c < 0x800) {
		 
		p[0] = 0xc0 | (c >> 6);
		p[1] = 0x80 | (c & 0x3f);
		return 2;
	} else if (c < 0x10000) {
		 
		p[0] = 0xe0 | (c >> 12);
		p[1] = 0x80 | ((c >> 6) & 0x3f);
		p[2] = 0x80 | (c & 0x3f);
		return 3;
	} else if (c < 0x110000) {
		 
		p[0] = 0xf0 | (c >> 18);
		p[1] = 0x80 | ((c >> 12) & 0x3f);
		p[2] = 0x80 | ((c >> 6) & 0x3f);
		p[3] = 0x80 | (c & 0x3f);
		return 4;
	} else {
		 
		p[0] = 0xef;
		p[1] = 0xbf;
		p[2] = 0xbd;
		return 3;
	}
}

 
int set_selection_user(const struct tiocl_selection __user *sel,
		       struct tty_struct *tty)
{
	struct tiocl_selection v;

	if (copy_from_user(&v, sel, sizeof(*sel)))
		return -EFAULT;

	return set_selection_kernel(&v, tty);
}

static int vc_selection_store_chars(struct vc_data *vc, bool unicode)
{
	char *bp, *obp;
	unsigned int i;

	 
	 
	bp = kmalloc_array((vc_sel.end - vc_sel.start) / 2 + 1, unicode ? 4 : 1,
			   GFP_KERNEL | __GFP_NOWARN);
	if (!bp) {
		printk(KERN_WARNING "selection: kmalloc() failed\n");
		clear_selection();
		return -ENOMEM;
	}
	kfree(vc_sel.buffer);
	vc_sel.buffer = bp;

	obp = bp;
	for (i = vc_sel.start; i <= vc_sel.end; i += 2) {
		u32 c = sel_pos(i, unicode);
		if (unicode)
			bp += store_utf8(c, bp);
		else
			*bp++ = c;
		if (!is_space_on_vt(c))
			obp = bp;
		if (!((i + 2) % vc->vc_size_row)) {
			 
			if (obp != bp) {
				bp = obp;
				*bp++ = '\r';
			}
			obp = bp;
		}
	}
	vc_sel.buf_len = bp - vc_sel.buffer;

	return 0;
}

static int vc_do_selection(struct vc_data *vc, unsigned short mode, int ps,
		int pe)
{
	int new_sel_start, new_sel_end, spc;
	bool unicode = vt_do_kdgkbmode(fg_console) == K_UNICODE;

	switch (mode) {
	case TIOCL_SELCHAR:	 
		new_sel_start = ps;
		new_sel_end = pe;
		break;
	case TIOCL_SELWORD:	 
		spc = is_space_on_vt(sel_pos(ps, unicode));
		for (new_sel_start = ps; ; ps -= 2) {
			if ((spc && !is_space_on_vt(sel_pos(ps, unicode))) ||
			    (!spc && !inword(sel_pos(ps, unicode))))
				break;
			new_sel_start = ps;
			if (!(ps % vc->vc_size_row))
				break;
		}

		spc = is_space_on_vt(sel_pos(pe, unicode));
		for (new_sel_end = pe; ; pe += 2) {
			if ((spc && !is_space_on_vt(sel_pos(pe, unicode))) ||
			    (!spc && !inword(sel_pos(pe, unicode))))
				break;
			new_sel_end = pe;
			if (!((pe + 2) % vc->vc_size_row))
				break;
		}
		break;
	case TIOCL_SELLINE:	 
		new_sel_start = rounddown(ps, vc->vc_size_row);
		new_sel_end = rounddown(pe, vc->vc_size_row) +
			vc->vc_size_row - 2;
		break;
	case TIOCL_SELPOINTER:
		highlight_pointer(pe);
		return 0;
	default:
		return -EINVAL;
	}

	 
	highlight_pointer(-1);

	 
	if (new_sel_end > new_sel_start &&
		!atedge(new_sel_end, vc->vc_size_row) &&
		is_space_on_vt(sel_pos(new_sel_end, unicode))) {
		for (pe = new_sel_end + 2; ; pe += 2)
			if (!is_space_on_vt(sel_pos(pe, unicode)) ||
			    atedge(pe, vc->vc_size_row))
				break;
		if (is_space_on_vt(sel_pos(pe, unicode)))
			new_sel_end = pe;
	}
	if (vc_sel.start == -1)	 
		highlight(new_sel_start, new_sel_end);
	else if (new_sel_start == vc_sel.start)
	{
		if (new_sel_end == vc_sel.end)	 
			return 0;
		else if (new_sel_end > vc_sel.end)	 
			highlight(vc_sel.end + 2, new_sel_end);
		else				 
			highlight(new_sel_end + 2, vc_sel.end);
	}
	else if (new_sel_end == vc_sel.end)
	{
		if (new_sel_start < vc_sel.start)  
			highlight(new_sel_start, vc_sel.start - 2);
		else				 
			highlight(vc_sel.start, new_sel_start - 2);
	}
	else	 
	{
		clear_selection();
		highlight(new_sel_start, new_sel_end);
	}
	vc_sel.start = new_sel_start;
	vc_sel.end = new_sel_end;

	return vc_selection_store_chars(vc, unicode);
}

static int vc_selection(struct vc_data *vc, struct tiocl_selection *v,
		struct tty_struct *tty)
{
	int ps, pe;

	poke_blanked_console();

	if (v->sel_mode == TIOCL_SELCLEAR) {
		 
		clear_selection();
		return 0;
	}

	v->xs = min_t(u16, v->xs - 1, vc->vc_cols - 1);
	v->ys = min_t(u16, v->ys - 1, vc->vc_rows - 1);
	v->xe = min_t(u16, v->xe - 1, vc->vc_cols - 1);
	v->ye = min_t(u16, v->ye - 1, vc->vc_rows - 1);

	if (mouse_reporting() && (v->sel_mode & TIOCL_SELMOUSEREPORT)) {
		mouse_report(tty, v->sel_mode & TIOCL_SELBUTTONMASK, v->xs,
			     v->ys);
		return 0;
	}

	ps = v->ys * vc->vc_size_row + (v->xs << 1);
	pe = v->ye * vc->vc_size_row + (v->xe << 1);
	if (ps > pe)	 
		swap(ps, pe);

	if (vc_sel.cons != vc) {
		clear_selection();
		vc_sel.cons = vc;
	}

	return vc_do_selection(vc, v->sel_mode, ps, pe);
}

int set_selection_kernel(struct tiocl_selection *v, struct tty_struct *tty)
{
	int ret;

	mutex_lock(&vc_sel.lock);
	console_lock();
	ret = vc_selection(vc_cons[fg_console].d, v, tty);
	console_unlock();
	mutex_unlock(&vc_sel.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(set_selection_kernel);

 
int paste_selection(struct tty_struct *tty)
{
	struct vc_data *vc = tty->driver_data;
	int	pasted = 0;
	size_t count;
	struct  tty_ldisc *ld;
	DECLARE_WAITQUEUE(wait, current);
	int ret = 0;

	console_lock();
	poke_blanked_console();
	console_unlock();

	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return -EIO;	 
	tty_buffer_lock_exclusive(&vc->port);

	add_wait_queue(&vc->paste_wait, &wait);
	mutex_lock(&vc_sel.lock);
	while (vc_sel.buffer && vc_sel.buf_len > pasted) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
		if (tty_throttled(tty)) {
			mutex_unlock(&vc_sel.lock);
			schedule();
			mutex_lock(&vc_sel.lock);
			continue;
		}
		__set_current_state(TASK_RUNNING);
		count = vc_sel.buf_len - pasted;
		count = tty_ldisc_receive_buf(ld, vc_sel.buffer + pasted, NULL,
					      count);
		pasted += count;
	}
	mutex_unlock(&vc_sel.lock);
	remove_wait_queue(&vc->paste_wait, &wait);
	__set_current_state(TASK_RUNNING);

	tty_buffer_unlock_exclusive(&vc->port);
	tty_ldisc_deref(ld);
	return ret;
}
EXPORT_SYMBOL_GPL(paste_selection);
