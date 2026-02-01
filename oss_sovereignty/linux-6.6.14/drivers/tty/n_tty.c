
 

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/jiffies.h>
#include <linux/math.h>
#include <linux/poll.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "tty.h"

 
#define WAKEUP_CHARS 256

 
#define TTY_THRESHOLD_THROTTLE		128  
#define TTY_THRESHOLD_UNTHROTTLE	128

 
#define ECHO_OP_START 0xff
#define ECHO_OP_MOVE_BACK_COL 0x80
#define ECHO_OP_SET_CANON_COL 0x81
#define ECHO_OP_ERASE_TAB 0x82

#define ECHO_COMMIT_WATERMARK	256
#define ECHO_BLOCK		256
#define ECHO_DISCARD_WATERMARK	N_TTY_BUF_SIZE - (ECHO_BLOCK + 32)


#undef N_TTY_TRACE
#ifdef N_TTY_TRACE
# define n_tty_trace(f, args...)	trace_printk(f, ##args)
#else
# define n_tty_trace(f, args...)	no_printk(f, ##args)
#endif

struct n_tty_data {
	 
	size_t read_head;
	size_t commit_head;
	size_t canon_head;
	size_t echo_head;
	size_t echo_commit;
	size_t echo_mark;
	DECLARE_BITMAP(char_map, 256);

	 
	unsigned long overrun_time;
	unsigned int num_overrun;

	 
	bool no_room;

	 
	unsigned char lnext:1, erasing:1, raw:1, real_raw:1, icanon:1;
	unsigned char push:1;

	 
	u8 read_buf[N_TTY_BUF_SIZE];
	DECLARE_BITMAP(read_flags, N_TTY_BUF_SIZE);
	u8 echo_buf[N_TTY_BUF_SIZE];

	 
	size_t read_tail;
	size_t line_start;

	 
	size_t lookahead_count;

	 
	unsigned int column;
	unsigned int canon_column;
	size_t echo_tail;

	struct mutex atomic_read_lock;
	struct mutex output_lock;
};

#define MASK(x) ((x) & (N_TTY_BUF_SIZE - 1))

static inline size_t read_cnt(struct n_tty_data *ldata)
{
	return ldata->read_head - ldata->read_tail;
}

static inline u8 read_buf(struct n_tty_data *ldata, size_t i)
{
	return ldata->read_buf[MASK(i)];
}

static inline u8 *read_buf_addr(struct n_tty_data *ldata, size_t i)
{
	return &ldata->read_buf[MASK(i)];
}

static inline u8 echo_buf(struct n_tty_data *ldata, size_t i)
{
	smp_rmb();  
	return ldata->echo_buf[MASK(i)];
}

static inline u8 *echo_buf_addr(struct n_tty_data *ldata, size_t i)
{
	return &ldata->echo_buf[MASK(i)];
}

 
static void zero_buffer(const struct tty_struct *tty, u8 *buffer, size_t size)
{
	if (L_ICANON(tty) && !L_ECHO(tty))
		memset(buffer, 0, size);
}

static void tty_copy(const struct tty_struct *tty, void *to, size_t tail,
		     size_t n)
{
	struct n_tty_data *ldata = tty->disc_data;
	size_t size = N_TTY_BUF_SIZE - tail;
	void *from = read_buf_addr(ldata, tail);

	if (n > size) {
		tty_audit_add_data(tty, from, size);
		memcpy(to, from, size);
		zero_buffer(tty, from, size);
		to += size;
		n -= size;
		from = ldata->read_buf;
	}

	tty_audit_add_data(tty, from, n);
	memcpy(to, from, n);
	zero_buffer(tty, from, n);
}

 
static void n_tty_kick_worker(const struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	 
	if (unlikely(READ_ONCE(ldata->no_room))) {
		WRITE_ONCE(ldata->no_room, 0);

		WARN_RATELIMIT(tty->port->itty == NULL,
				"scheduling with invalid itty\n");
		 
		WARN_RATELIMIT(test_bit(TTY_LDISC_HALTED, &tty->flags),
			       "scheduling buffer work for halted ldisc\n");
		tty_buffer_restart_work(tty->port);
	}
}

static ssize_t chars_in_buffer(const struct tty_struct *tty)
{
	const struct n_tty_data *ldata = tty->disc_data;
	size_t head = ldata->icanon ? ldata->canon_head : ldata->commit_head;

	return head - ldata->read_tail;
}

 
static void n_tty_write_wakeup(struct tty_struct *tty)
{
	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	kill_fasync(&tty->fasync, SIGIO, POLL_OUT);
}

static void n_tty_check_throttle(struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	 
	if (ldata->icanon && ldata->canon_head == ldata->read_tail)
		return;

	while (1) {
		int throttled;
		tty_set_flow_change(tty, TTY_THROTTLE_SAFE);
		if (N_TTY_BUF_SIZE - read_cnt(ldata) >= TTY_THRESHOLD_THROTTLE)
			break;
		throttled = tty_throttle_safe(tty);
		if (!throttled)
			break;
	}
	__tty_set_flow_change(tty, 0);
}

static void n_tty_check_unthrottle(struct tty_struct *tty)
{
	if (tty->driver->type == TTY_DRIVER_TYPE_PTY) {
		if (chars_in_buffer(tty) > TTY_THRESHOLD_UNTHROTTLE)
			return;
		n_tty_kick_worker(tty);
		tty_wakeup(tty->link);
		return;
	}

	 

	while (1) {
		int unthrottled;
		tty_set_flow_change(tty, TTY_UNTHROTTLE_SAFE);
		if (chars_in_buffer(tty) > TTY_THRESHOLD_UNTHROTTLE)
			break;
		n_tty_kick_worker(tty);
		unthrottled = tty_unthrottle_safe(tty);
		if (!unthrottled)
			break;
	}
	__tty_set_flow_change(tty, 0);
}

 
static inline void put_tty_queue(u8 c, struct n_tty_data *ldata)
{
	*read_buf_addr(ldata, ldata->read_head) = c;
	ldata->read_head++;
}

 
static void reset_buffer_flags(struct n_tty_data *ldata)
{
	ldata->read_head = ldata->canon_head = ldata->read_tail = 0;
	ldata->commit_head = 0;
	ldata->line_start = 0;

	ldata->erasing = 0;
	bitmap_zero(ldata->read_flags, N_TTY_BUF_SIZE);
	ldata->push = 0;

	ldata->lookahead_count = 0;
}

static void n_tty_packet_mode_flush(struct tty_struct *tty)
{
	unsigned long flags;

	if (tty->link->ctrl.packet) {
		spin_lock_irqsave(&tty->ctrl.lock, flags);
		tty->ctrl.pktstatus |= TIOCPKT_FLUSHREAD;
		spin_unlock_irqrestore(&tty->ctrl.lock, flags);
		wake_up_interruptible(&tty->link->read_wait);
	}
}

 
static void n_tty_flush_buffer(struct tty_struct *tty)
{
	down_write(&tty->termios_rwsem);
	reset_buffer_flags(tty->disc_data);
	n_tty_kick_worker(tty);

	if (tty->link)
		n_tty_packet_mode_flush(tty);
	up_write(&tty->termios_rwsem);
}

 
static inline int is_utf8_continuation(u8 c)
{
	return (c & 0xc0) == 0x80;
}

 
static inline int is_continuation(u8 c, const struct tty_struct *tty)
{
	return I_IUTF8(tty) && is_utf8_continuation(c);
}

 
static int do_output_char(u8 c, struct tty_struct *tty, int space)
{
	struct n_tty_data *ldata = tty->disc_data;
	int	spaces;

	if (!space)
		return -1;

	switch (c) {
	case '\n':
		if (O_ONLRET(tty))
			ldata->column = 0;
		if (O_ONLCR(tty)) {
			if (space < 2)
				return -1;
			ldata->canon_column = ldata->column = 0;
			tty->ops->write(tty, "\r\n", 2);
			return 2;
		}
		ldata->canon_column = ldata->column;
		break;
	case '\r':
		if (O_ONOCR(tty) && ldata->column == 0)
			return 0;
		if (O_OCRNL(tty)) {
			c = '\n';
			if (O_ONLRET(tty))
				ldata->canon_column = ldata->column = 0;
			break;
		}
		ldata->canon_column = ldata->column = 0;
		break;
	case '\t':
		spaces = 8 - (ldata->column & 7);
		if (O_TABDLY(tty) == XTABS) {
			if (space < spaces)
				return -1;
			ldata->column += spaces;
			tty->ops->write(tty, "        ", spaces);
			return spaces;
		}
		ldata->column += spaces;
		break;
	case '\b':
		if (ldata->column > 0)
			ldata->column--;
		break;
	default:
		if (!iscntrl(c)) {
			if (O_OLCUC(tty))
				c = toupper(c);
			if (!is_continuation(c, tty))
				ldata->column++;
		}
		break;
	}

	tty_put_char(tty, c);
	return 1;
}

 
static int process_output(u8 c, struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;
	int	space, retval;

	mutex_lock(&ldata->output_lock);

	space = tty_write_room(tty);
	retval = do_output_char(c, tty, space);

	mutex_unlock(&ldata->output_lock);
	if (retval < 0)
		return -1;
	else
		return 0;
}

 
static ssize_t process_output_block(struct tty_struct *tty,
				    const u8 *buf, unsigned int nr)
{
	struct n_tty_data *ldata = tty->disc_data;
	int	space;
	int	i;
	const u8 *cp;

	mutex_lock(&ldata->output_lock);

	space = tty_write_room(tty);
	if (space <= 0) {
		mutex_unlock(&ldata->output_lock);
		return space;
	}
	if (nr > space)
		nr = space;

	for (i = 0, cp = buf; i < nr; i++, cp++) {
		u8 c = *cp;

		switch (c) {
		case '\n':
			if (O_ONLRET(tty))
				ldata->column = 0;
			if (O_ONLCR(tty))
				goto break_out;
			ldata->canon_column = ldata->column;
			break;
		case '\r':
			if (O_ONOCR(tty) && ldata->column == 0)
				goto break_out;
			if (O_OCRNL(tty))
				goto break_out;
			ldata->canon_column = ldata->column = 0;
			break;
		case '\t':
			goto break_out;
		case '\b':
			if (ldata->column > 0)
				ldata->column--;
			break;
		default:
			if (!iscntrl(c)) {
				if (O_OLCUC(tty))
					goto break_out;
				if (!is_continuation(c, tty))
					ldata->column++;
			}
			break;
		}
	}
break_out:
	i = tty->ops->write(tty, buf, i);

	mutex_unlock(&ldata->output_lock);
	return i;
}

static int n_tty_process_echo_ops(struct tty_struct *tty, size_t *tail,
				  int space)
{
	struct n_tty_data *ldata = tty->disc_data;
	u8 op;

	 
	if (MASK(ldata->echo_commit) == MASK(*tail + 1))
		return -ENODATA;

	 
	op = echo_buf(ldata, *tail + 1);

	switch (op) {
	case ECHO_OP_ERASE_TAB: {
		unsigned int num_chars, num_bs;

		if (MASK(ldata->echo_commit) == MASK(*tail + 2))
			return -ENODATA;

		num_chars = echo_buf(ldata, *tail + 2);

		 
		if (!(num_chars & 0x80))
			num_chars += ldata->canon_column;
		num_bs = 8 - (num_chars & 7);

		if (num_bs > space)
			return -ENOSPC;

		space -= num_bs;
		while (num_bs--) {
			tty_put_char(tty, '\b');
			if (ldata->column > 0)
				ldata->column--;
		}
		*tail += 3;
		break;
	}
	case ECHO_OP_SET_CANON_COL:
		ldata->canon_column = ldata->column;
		*tail += 2;
		break;

	case ECHO_OP_MOVE_BACK_COL:
		if (ldata->column > 0)
			ldata->column--;
		*tail += 2;
		break;

	case ECHO_OP_START:
		 
		if (!space)
			return -ENOSPC;

		tty_put_char(tty, ECHO_OP_START);
		ldata->column++;
		space--;
		*tail += 2;
		break;

	default:
		 
		if (space < 2)
			return -ENOSPC;

		tty_put_char(tty, '^');
		tty_put_char(tty, op ^ 0100);
		ldata->column += 2;
		space -= 2;
		*tail += 2;
		break;
	}

	return space;
}

 
static size_t __process_echoes(struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;
	int	space, old_space;
	size_t tail;
	u8 c;

	old_space = space = tty_write_room(tty);

	tail = ldata->echo_tail;
	while (MASK(ldata->echo_commit) != MASK(tail)) {
		c = echo_buf(ldata, tail);
		if (c == ECHO_OP_START) {
			int ret = n_tty_process_echo_ops(tty, &tail, space);
			if (ret == -ENODATA)
				goto not_yet_stored;
			if (ret < 0)
				break;
			space = ret;
		} else {
			if (O_OPOST(tty)) {
				int retval = do_output_char(c, tty, space);
				if (retval < 0)
					break;
				space -= retval;
			} else {
				if (!space)
					break;
				tty_put_char(tty, c);
				space -= 1;
			}
			tail += 1;
		}
	}

	 
	while (ldata->echo_commit > tail &&
	       ldata->echo_commit - tail >= ECHO_DISCARD_WATERMARK) {
		if (echo_buf(ldata, tail) == ECHO_OP_START) {
			if (echo_buf(ldata, tail + 1) == ECHO_OP_ERASE_TAB)
				tail += 3;
			else
				tail += 2;
		} else
			tail++;
	}

 not_yet_stored:
	ldata->echo_tail = tail;
	return old_space - space;
}

static void commit_echoes(struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;
	size_t nr, old, echoed;
	size_t head;

	mutex_lock(&ldata->output_lock);
	head = ldata->echo_head;
	ldata->echo_mark = head;
	old = ldata->echo_commit - ldata->echo_tail;

	 
	nr = head - ldata->echo_tail;
	if (nr < ECHO_COMMIT_WATERMARK ||
	    (nr % ECHO_BLOCK > old % ECHO_BLOCK)) {
		mutex_unlock(&ldata->output_lock);
		return;
	}

	ldata->echo_commit = head;
	echoed = __process_echoes(tty);
	mutex_unlock(&ldata->output_lock);

	if (echoed && tty->ops->flush_chars)
		tty->ops->flush_chars(tty);
}

static void process_echoes(struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;
	size_t echoed;

	if (ldata->echo_mark == ldata->echo_tail)
		return;

	mutex_lock(&ldata->output_lock);
	ldata->echo_commit = ldata->echo_mark;
	echoed = __process_echoes(tty);
	mutex_unlock(&ldata->output_lock);

	if (echoed && tty->ops->flush_chars)
		tty->ops->flush_chars(tty);
}

 
static void flush_echoes(struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	if ((!L_ECHO(tty) && !L_ECHONL(tty)) ||
	    ldata->echo_commit == ldata->echo_head)
		return;

	mutex_lock(&ldata->output_lock);
	ldata->echo_commit = ldata->echo_head;
	__process_echoes(tty);
	mutex_unlock(&ldata->output_lock);
}

 
static inline void add_echo_byte(u8 c, struct n_tty_data *ldata)
{
	*echo_buf_addr(ldata, ldata->echo_head) = c;
	smp_wmb();  
	ldata->echo_head++;
}

 
static void echo_move_back_col(struct n_tty_data *ldata)
{
	add_echo_byte(ECHO_OP_START, ldata);
	add_echo_byte(ECHO_OP_MOVE_BACK_COL, ldata);
}

 
static void echo_set_canon_col(struct n_tty_data *ldata)
{
	add_echo_byte(ECHO_OP_START, ldata);
	add_echo_byte(ECHO_OP_SET_CANON_COL, ldata);
}

 
static void echo_erase_tab(unsigned int num_chars, int after_tab,
			   struct n_tty_data *ldata)
{
	add_echo_byte(ECHO_OP_START, ldata);
	add_echo_byte(ECHO_OP_ERASE_TAB, ldata);

	 
	num_chars &= 7;

	 
	if (after_tab)
		num_chars |= 0x80;

	add_echo_byte(num_chars, ldata);
}

 
static void echo_char_raw(u8 c, struct n_tty_data *ldata)
{
	if (c == ECHO_OP_START) {
		add_echo_byte(ECHO_OP_START, ldata);
		add_echo_byte(ECHO_OP_START, ldata);
	} else {
		add_echo_byte(c, ldata);
	}
}

 
static void echo_char(u8 c, const struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (c == ECHO_OP_START) {
		add_echo_byte(ECHO_OP_START, ldata);
		add_echo_byte(ECHO_OP_START, ldata);
	} else {
		if (L_ECHOCTL(tty) && iscntrl(c) && c != '\t')
			add_echo_byte(ECHO_OP_START, ldata);
		add_echo_byte(c, ldata);
	}
}

 
static inline void finish_erasing(struct n_tty_data *ldata)
{
	if (ldata->erasing) {
		echo_char_raw('/', ldata);
		ldata->erasing = 0;
	}
}

 
static void eraser(u8 c, const struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;
	enum { ERASE, WERASE, KILL } kill_type;
	size_t head;
	size_t cnt;
	int seen_alnums;

	if (ldata->read_head == ldata->canon_head) {
		   
		return;
	}
	if (c == ERASE_CHAR(tty))
		kill_type = ERASE;
	else if (c == WERASE_CHAR(tty))
		kill_type = WERASE;
	else {
		if (!L_ECHO(tty)) {
			ldata->read_head = ldata->canon_head;
			return;
		}
		if (!L_ECHOK(tty) || !L_ECHOKE(tty) || !L_ECHOE(tty)) {
			ldata->read_head = ldata->canon_head;
			finish_erasing(ldata);
			echo_char(KILL_CHAR(tty), tty);
			 
			if (L_ECHOK(tty))
				echo_char_raw('\n', ldata);
			return;
		}
		kill_type = KILL;
	}

	seen_alnums = 0;
	while (MASK(ldata->read_head) != MASK(ldata->canon_head)) {
		head = ldata->read_head;

		 
		do {
			head--;
			c = read_buf(ldata, head);
		} while (is_continuation(c, tty) &&
			 MASK(head) != MASK(ldata->canon_head));

		 
		if (is_continuation(c, tty))
			break;

		if (kill_type == WERASE) {
			 
			if (isalnum(c) || c == '_')
				seen_alnums++;
			else if (seen_alnums)
				break;
		}
		cnt = ldata->read_head - head;
		ldata->read_head = head;
		if (L_ECHO(tty)) {
			if (L_ECHOPRT(tty)) {
				if (!ldata->erasing) {
					echo_char_raw('\\', ldata);
					ldata->erasing = 1;
				}
				 
				echo_char(c, tty);
				while (--cnt > 0) {
					head++;
					echo_char_raw(read_buf(ldata, head), ldata);
					echo_move_back_col(ldata);
				}
			} else if (kill_type == ERASE && !L_ECHOE(tty)) {
				echo_char(ERASE_CHAR(tty), tty);
			} else if (c == '\t') {
				unsigned int num_chars = 0;
				int after_tab = 0;
				size_t tail = ldata->read_head;

				 
				while (MASK(tail) != MASK(ldata->canon_head)) {
					tail--;
					c = read_buf(ldata, tail);
					if (c == '\t') {
						after_tab = 1;
						break;
					} else if (iscntrl(c)) {
						if (L_ECHOCTL(tty))
							num_chars += 2;
					} else if (!is_continuation(c, tty)) {
						num_chars++;
					}
				}
				echo_erase_tab(num_chars, after_tab, ldata);
			} else {
				if (iscntrl(c) && L_ECHOCTL(tty)) {
					echo_char_raw('\b', ldata);
					echo_char_raw(' ', ldata);
					echo_char_raw('\b', ldata);
				}
				if (!iscntrl(c) || L_ECHOCTL(tty)) {
					echo_char_raw('\b', ldata);
					echo_char_raw(' ', ldata);
					echo_char_raw('\b', ldata);
				}
			}
		}
		if (kill_type == ERASE)
			break;
	}
	if (ldata->read_head == ldata->canon_head && L_ECHO(tty))
		finish_erasing(ldata);
}


static void __isig(int sig, struct tty_struct *tty)
{
	struct pid *tty_pgrp = tty_get_pgrp(tty);
	if (tty_pgrp) {
		kill_pgrp(tty_pgrp, sig, 1);
		put_pid(tty_pgrp);
	}
}

 
static void isig(int sig, struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (L_NOFLSH(tty)) {
		 
		__isig(sig, tty);

	} else {  
		up_read(&tty->termios_rwsem);
		down_write(&tty->termios_rwsem);

		__isig(sig, tty);

		 
		mutex_lock(&ldata->output_lock);
		ldata->echo_head = ldata->echo_tail = 0;
		ldata->echo_mark = ldata->echo_commit = 0;
		mutex_unlock(&ldata->output_lock);

		 
		tty_driver_flush_buffer(tty);

		 
		reset_buffer_flags(tty->disc_data);

		 
		if (tty->link)
			n_tty_packet_mode_flush(tty);

		up_write(&tty->termios_rwsem);
		down_read(&tty->termios_rwsem);
	}
}

 
static void n_tty_receive_break(struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (I_IGNBRK(tty))
		return;
	if (I_BRKINT(tty)) {
		isig(SIGINT, tty);
		return;
	}
	if (I_PARMRK(tty)) {
		put_tty_queue('\377', ldata);
		put_tty_queue('\0', ldata);
	}
	put_tty_queue('\0', ldata);
}

 
static void n_tty_receive_overrun(const struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	ldata->num_overrun++;
	if (time_is_before_jiffies(ldata->overrun_time + HZ)) {
		tty_warn(tty, "%u input overrun(s)\n", ldata->num_overrun);
		ldata->overrun_time = jiffies;
		ldata->num_overrun = 0;
	}
}

 
static void n_tty_receive_parity_error(const struct tty_struct *tty,
				       u8 c)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (I_INPCK(tty)) {
		if (I_IGNPAR(tty))
			return;
		if (I_PARMRK(tty)) {
			put_tty_queue('\377', ldata);
			put_tty_queue('\0', ldata);
			put_tty_queue(c, ldata);
		} else
			put_tty_queue('\0', ldata);
	} else
		put_tty_queue(c, ldata);
}

static void
n_tty_receive_signal_char(struct tty_struct *tty, int signal, u8 c)
{
	isig(signal, tty);
	if (I_IXON(tty))
		start_tty(tty);
	if (L_ECHO(tty)) {
		echo_char(c, tty);
		commit_echoes(tty);
	} else
		process_echoes(tty);
}

static bool n_tty_is_char_flow_ctrl(struct tty_struct *tty, u8 c)
{
	return c == START_CHAR(tty) || c == STOP_CHAR(tty);
}

 
static bool n_tty_receive_char_flow_ctrl(struct tty_struct *tty, u8 c,
					 bool lookahead_done)
{
	if (!n_tty_is_char_flow_ctrl(tty, c))
		return false;

	if (lookahead_done)
		return true;

	if (c == START_CHAR(tty)) {
		start_tty(tty);
		process_echoes(tty);
		return true;
	}

	 
	stop_tty(tty);
	return true;
}

static void n_tty_receive_handle_newline(struct tty_struct *tty, u8 c)
{
	struct n_tty_data *ldata = tty->disc_data;

	set_bit(MASK(ldata->read_head), ldata->read_flags);
	put_tty_queue(c, ldata);
	smp_store_release(&ldata->canon_head, ldata->read_head);
	kill_fasync(&tty->fasync, SIGIO, POLL_IN);
	wake_up_interruptible_poll(&tty->read_wait, EPOLLIN | EPOLLRDNORM);
}

static bool n_tty_receive_char_canon(struct tty_struct *tty, u8 c)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (c == ERASE_CHAR(tty) || c == KILL_CHAR(tty) ||
	    (c == WERASE_CHAR(tty) && L_IEXTEN(tty))) {
		eraser(c, tty);
		commit_echoes(tty);

		return true;
	}

	if (c == LNEXT_CHAR(tty) && L_IEXTEN(tty)) {
		ldata->lnext = 1;
		if (L_ECHO(tty)) {
			finish_erasing(ldata);
			if (L_ECHOCTL(tty)) {
				echo_char_raw('^', ldata);
				echo_char_raw('\b', ldata);
				commit_echoes(tty);
			}
		}

		return true;
	}

	if (c == REPRINT_CHAR(tty) && L_ECHO(tty) && L_IEXTEN(tty)) {
		size_t tail = ldata->canon_head;

		finish_erasing(ldata);
		echo_char(c, tty);
		echo_char_raw('\n', ldata);
		while (MASK(tail) != MASK(ldata->read_head)) {
			echo_char(read_buf(ldata, tail), tty);
			tail++;
		}
		commit_echoes(tty);

		return true;
	}

	if (c == '\n') {
		if (L_ECHO(tty) || L_ECHONL(tty)) {
			echo_char_raw('\n', ldata);
			commit_echoes(tty);
		}
		n_tty_receive_handle_newline(tty, c);

		return true;
	}

	if (c == EOF_CHAR(tty)) {
		c = __DISABLED_CHAR;
		n_tty_receive_handle_newline(tty, c);

		return true;
	}

	if ((c == EOL_CHAR(tty)) ||
	    (c == EOL2_CHAR(tty) && L_IEXTEN(tty))) {
		 
		if (L_ECHO(tty)) {
			 
			if (ldata->canon_head == ldata->read_head)
				echo_set_canon_col(ldata);
			echo_char(c, tty);
			commit_echoes(tty);
		}
		 
		if (c == '\377' && I_PARMRK(tty))
			put_tty_queue(c, ldata);

		n_tty_receive_handle_newline(tty, c);

		return true;
	}

	return false;
}

static void n_tty_receive_char_special(struct tty_struct *tty, u8 c,
				       bool lookahead_done)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (I_IXON(tty) && n_tty_receive_char_flow_ctrl(tty, c, lookahead_done))
		return;

	if (L_ISIG(tty)) {
		if (c == INTR_CHAR(tty)) {
			n_tty_receive_signal_char(tty, SIGINT, c);
			return;
		} else if (c == QUIT_CHAR(tty)) {
			n_tty_receive_signal_char(tty, SIGQUIT, c);
			return;
		} else if (c == SUSP_CHAR(tty)) {
			n_tty_receive_signal_char(tty, SIGTSTP, c);
			return;
		}
	}

	if (tty->flow.stopped && !tty->flow.tco_stopped && I_IXON(tty) && I_IXANY(tty)) {
		start_tty(tty);
		process_echoes(tty);
	}

	if (c == '\r') {
		if (I_IGNCR(tty))
			return;
		if (I_ICRNL(tty))
			c = '\n';
	} else if (c == '\n' && I_INLCR(tty))
		c = '\r';

	if (ldata->icanon && n_tty_receive_char_canon(tty, c))
		return;

	if (L_ECHO(tty)) {
		finish_erasing(ldata);
		if (c == '\n')
			echo_char_raw('\n', ldata);
		else {
			 
			if (ldata->canon_head == ldata->read_head)
				echo_set_canon_col(ldata);
			echo_char(c, tty);
		}
		commit_echoes(tty);
	}

	 
	if (c == '\377' && I_PARMRK(tty))
		put_tty_queue(c, ldata);

	put_tty_queue(c, ldata);
}

 
static void n_tty_receive_char(struct tty_struct *tty, u8 c)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (tty->flow.stopped && !tty->flow.tco_stopped && I_IXON(tty) && I_IXANY(tty)) {
		start_tty(tty);
		process_echoes(tty);
	}
	if (L_ECHO(tty)) {
		finish_erasing(ldata);
		 
		if (ldata->canon_head == ldata->read_head)
			echo_set_canon_col(ldata);
		echo_char(c, tty);
		commit_echoes(tty);
	}
	 
	if (c == '\377' && I_PARMRK(tty))
		put_tty_queue(c, ldata);
	put_tty_queue(c, ldata);
}

static void n_tty_receive_char_closing(struct tty_struct *tty, u8 c,
				       bool lookahead_done)
{
	if (I_ISTRIP(tty))
		c &= 0x7f;
	if (I_IUCLC(tty) && L_IEXTEN(tty))
		c = tolower(c);

	if (I_IXON(tty)) {
		if (!n_tty_receive_char_flow_ctrl(tty, c, lookahead_done) &&
		    tty->flow.stopped && !tty->flow.tco_stopped && I_IXANY(tty) &&
		    c != INTR_CHAR(tty) && c != QUIT_CHAR(tty) &&
		    c != SUSP_CHAR(tty)) {
			start_tty(tty);
			process_echoes(tty);
		}
	}
}

static void
n_tty_receive_char_flagged(struct tty_struct *tty, u8 c, u8 flag)
{
	switch (flag) {
	case TTY_BREAK:
		n_tty_receive_break(tty);
		break;
	case TTY_PARITY:
	case TTY_FRAME:
		n_tty_receive_parity_error(tty, c);
		break;
	case TTY_OVERRUN:
		n_tty_receive_overrun(tty);
		break;
	default:
		tty_err(tty, "unknown flag %u\n", flag);
		break;
	}
}

static void
n_tty_receive_char_lnext(struct tty_struct *tty, u8 c, u8 flag)
{
	struct n_tty_data *ldata = tty->disc_data;

	ldata->lnext = 0;
	if (likely(flag == TTY_NORMAL)) {
		if (I_ISTRIP(tty))
			c &= 0x7f;
		if (I_IUCLC(tty) && L_IEXTEN(tty))
			c = tolower(c);
		n_tty_receive_char(tty, c);
	} else
		n_tty_receive_char_flagged(tty, c, flag);
}

 
static void n_tty_lookahead_flow_ctrl(struct tty_struct *tty, const u8 *cp,
				      const u8 *fp, size_t count)
{
	struct n_tty_data *ldata = tty->disc_data;
	u8 flag = TTY_NORMAL;

	ldata->lookahead_count += count;

	if (!I_IXON(tty))
		return;

	while (count--) {
		if (fp)
			flag = *fp++;
		if (likely(flag == TTY_NORMAL))
			n_tty_receive_char_flow_ctrl(tty, *cp, false);
		cp++;
	}
}

static void
n_tty_receive_buf_real_raw(const struct tty_struct *tty, const u8 *cp,
			   size_t count)
{
	struct n_tty_data *ldata = tty->disc_data;

	 
	for (unsigned int i = 0; i < 2; i++) {
		size_t head = MASK(ldata->read_head);
		size_t n = min(count, N_TTY_BUF_SIZE - head);

		memcpy(read_buf_addr(ldata, head), cp, n);

		ldata->read_head += n;
		cp += n;
		count -= n;
	}
}

static void
n_tty_receive_buf_raw(struct tty_struct *tty, const u8 *cp, const u8 *fp,
		      size_t count)
{
	struct n_tty_data *ldata = tty->disc_data;
	u8 flag = TTY_NORMAL;

	while (count--) {
		if (fp)
			flag = *fp++;
		if (likely(flag == TTY_NORMAL))
			put_tty_queue(*cp++, ldata);
		else
			n_tty_receive_char_flagged(tty, *cp++, flag);
	}
}

static void
n_tty_receive_buf_closing(struct tty_struct *tty, const u8 *cp, const u8 *fp,
			  size_t count, bool lookahead_done)
{
	u8 flag = TTY_NORMAL;

	while (count--) {
		if (fp)
			flag = *fp++;
		if (likely(flag == TTY_NORMAL))
			n_tty_receive_char_closing(tty, *cp++, lookahead_done);
	}
}

static void n_tty_receive_buf_standard(struct tty_struct *tty, const u8 *cp,
				       const u8 *fp, size_t count,
				       bool lookahead_done)
{
	struct n_tty_data *ldata = tty->disc_data;
	u8 flag = TTY_NORMAL;

	while (count--) {
		u8 c = *cp++;

		if (fp)
			flag = *fp++;

		if (ldata->lnext) {
			n_tty_receive_char_lnext(tty, c, flag);
			continue;
		}

		if (unlikely(flag != TTY_NORMAL)) {
			n_tty_receive_char_flagged(tty, c, flag);
			continue;
		}

		if (I_ISTRIP(tty))
			c &= 0x7f;
		if (I_IUCLC(tty) && L_IEXTEN(tty))
			c = tolower(c);
		if (L_EXTPROC(tty)) {
			put_tty_queue(c, ldata);
			continue;
		}

		if (test_bit(c, ldata->char_map))
			n_tty_receive_char_special(tty, c, lookahead_done);
		else
			n_tty_receive_char(tty, c);
	}
}

static void __receive_buf(struct tty_struct *tty, const u8 *cp, const u8 *fp,
			  size_t count)
{
	struct n_tty_data *ldata = tty->disc_data;
	bool preops = I_ISTRIP(tty) || (I_IUCLC(tty) && L_IEXTEN(tty));
	size_t la_count = min(ldata->lookahead_count, count);

	if (ldata->real_raw)
		n_tty_receive_buf_real_raw(tty, cp, count);
	else if (ldata->raw || (L_EXTPROC(tty) && !preops))
		n_tty_receive_buf_raw(tty, cp, fp, count);
	else if (tty->closing && !L_EXTPROC(tty)) {
		if (la_count > 0)
			n_tty_receive_buf_closing(tty, cp, fp, la_count, true);
		if (count > la_count)
			n_tty_receive_buf_closing(tty, cp, fp, count - la_count, false);
	} else {
		if (la_count > 0)
			n_tty_receive_buf_standard(tty, cp, fp, la_count, true);
		if (count > la_count)
			n_tty_receive_buf_standard(tty, cp, fp, count - la_count, false);

		flush_echoes(tty);
		if (tty->ops->flush_chars)
			tty->ops->flush_chars(tty);
	}

	ldata->lookahead_count -= la_count;

	if (ldata->icanon && !L_EXTPROC(tty))
		return;

	 
	smp_store_release(&ldata->commit_head, ldata->read_head);

	if (read_cnt(ldata)) {
		kill_fasync(&tty->fasync, SIGIO, POLL_IN);
		wake_up_interruptible_poll(&tty->read_wait, EPOLLIN | EPOLLRDNORM);
	}
}

 
static size_t
n_tty_receive_buf_common(struct tty_struct *tty, const u8 *cp, const u8 *fp,
			 size_t count, bool flow)
{
	struct n_tty_data *ldata = tty->disc_data;
	size_t n, rcvd = 0;
	int room, overflow;

	down_read(&tty->termios_rwsem);

	do {
		 
		size_t tail = smp_load_acquire(&ldata->read_tail);

		room = N_TTY_BUF_SIZE - (ldata->read_head - tail);
		if (I_PARMRK(tty))
			room = DIV_ROUND_UP(room, 3);
		room--;
		if (room <= 0) {
			overflow = ldata->icanon && ldata->canon_head == tail;
			if (overflow && room < 0)
				ldata->read_head--;
			room = overflow;
			WRITE_ONCE(ldata->no_room, flow && !room);
		} else
			overflow = 0;

		n = min_t(size_t, count, room);
		if (!n)
			break;

		 
		if (!overflow || !fp || *fp != TTY_PARITY)
			__receive_buf(tty, cp, fp, n);

		cp += n;
		if (fp)
			fp += n;
		count -= n;
		rcvd += n;
	} while (!test_bit(TTY_LDISC_CHANGING, &tty->flags));

	tty->receive_room = room;

	 
	if (tty->driver->type == TTY_DRIVER_TYPE_PTY) {
		if (overflow) {
			tty_set_flow_change(tty, TTY_UNTHROTTLE_SAFE);
			tty_unthrottle_safe(tty);
			__tty_set_flow_change(tty, 0);
		}
	} else
		n_tty_check_throttle(tty);

	if (unlikely(ldata->no_room)) {
		 
		smp_mb();
		if (!chars_in_buffer(tty))
			n_tty_kick_worker(tty);
	}

	up_read(&tty->termios_rwsem);

	return rcvd;
}

static void n_tty_receive_buf(struct tty_struct *tty, const u8 *cp,
			      const u8 *fp, size_t count)
{
	n_tty_receive_buf_common(tty, cp, fp, count, false);
}

static size_t n_tty_receive_buf2(struct tty_struct *tty, const u8 *cp,
				 const u8 *fp, size_t count)
{
	return n_tty_receive_buf_common(tty, cp, fp, count, true);
}

 
static void n_tty_set_termios(struct tty_struct *tty, const struct ktermios *old)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (!old || (old->c_lflag ^ tty->termios.c_lflag) & (ICANON | EXTPROC)) {
		bitmap_zero(ldata->read_flags, N_TTY_BUF_SIZE);
		ldata->line_start = ldata->read_tail;
		if (!L_ICANON(tty) || !read_cnt(ldata)) {
			ldata->canon_head = ldata->read_tail;
			ldata->push = 0;
		} else {
			set_bit(MASK(ldata->read_head - 1), ldata->read_flags);
			ldata->canon_head = ldata->read_head;
			ldata->push = 1;
		}
		ldata->commit_head = ldata->read_head;
		ldata->erasing = 0;
		ldata->lnext = 0;
	}

	ldata->icanon = (L_ICANON(tty) != 0);

	if (I_ISTRIP(tty) || I_IUCLC(tty) || I_IGNCR(tty) ||
	    I_ICRNL(tty) || I_INLCR(tty) || L_ICANON(tty) ||
	    I_IXON(tty) || L_ISIG(tty) || L_ECHO(tty) ||
	    I_PARMRK(tty)) {
		bitmap_zero(ldata->char_map, 256);

		if (I_IGNCR(tty) || I_ICRNL(tty))
			set_bit('\r', ldata->char_map);
		if (I_INLCR(tty))
			set_bit('\n', ldata->char_map);

		if (L_ICANON(tty)) {
			set_bit(ERASE_CHAR(tty), ldata->char_map);
			set_bit(KILL_CHAR(tty), ldata->char_map);
			set_bit(EOF_CHAR(tty), ldata->char_map);
			set_bit('\n', ldata->char_map);
			set_bit(EOL_CHAR(tty), ldata->char_map);
			if (L_IEXTEN(tty)) {
				set_bit(WERASE_CHAR(tty), ldata->char_map);
				set_bit(LNEXT_CHAR(tty), ldata->char_map);
				set_bit(EOL2_CHAR(tty), ldata->char_map);
				if (L_ECHO(tty))
					set_bit(REPRINT_CHAR(tty),
						ldata->char_map);
			}
		}
		if (I_IXON(tty)) {
			set_bit(START_CHAR(tty), ldata->char_map);
			set_bit(STOP_CHAR(tty), ldata->char_map);
		}
		if (L_ISIG(tty)) {
			set_bit(INTR_CHAR(tty), ldata->char_map);
			set_bit(QUIT_CHAR(tty), ldata->char_map);
			set_bit(SUSP_CHAR(tty), ldata->char_map);
		}
		clear_bit(__DISABLED_CHAR, ldata->char_map);
		ldata->raw = 0;
		ldata->real_raw = 0;
	} else {
		ldata->raw = 1;
		if ((I_IGNBRK(tty) || (!I_BRKINT(tty) && !I_PARMRK(tty))) &&
		    (I_IGNPAR(tty) || !I_INPCK(tty)) &&
		    (tty->driver->flags & TTY_DRIVER_REAL_RAW))
			ldata->real_raw = 1;
		else
			ldata->real_raw = 0;
	}
	 
	if (!I_IXON(tty) && old && (old->c_iflag & IXON) && !tty->flow.tco_stopped) {
		start_tty(tty);
		process_echoes(tty);
	}

	 
	wake_up_interruptible(&tty->write_wait);
	wake_up_interruptible(&tty->read_wait);
}

 
static void n_tty_close(struct tty_struct *tty)
{
	struct n_tty_data *ldata = tty->disc_data;

	if (tty->link)
		n_tty_packet_mode_flush(tty);

	down_write(&tty->termios_rwsem);
	vfree(ldata);
	tty->disc_data = NULL;
	up_write(&tty->termios_rwsem);
}

 
static int n_tty_open(struct tty_struct *tty)
{
	struct n_tty_data *ldata;

	 
	ldata = vzalloc(sizeof(*ldata));
	if (!ldata)
		return -ENOMEM;

	ldata->overrun_time = jiffies;
	mutex_init(&ldata->atomic_read_lock);
	mutex_init(&ldata->output_lock);

	tty->disc_data = ldata;
	tty->closing = 0;
	 
	clear_bit(TTY_LDISC_HALTED, &tty->flags);
	n_tty_set_termios(tty, NULL);
	tty_unthrottle(tty);
	return 0;
}

static inline int input_available_p(const struct tty_struct *tty, int poll)
{
	const struct n_tty_data *ldata = tty->disc_data;
	int amt = poll && !TIME_CHAR(tty) && MIN_CHAR(tty) ? MIN_CHAR(tty) : 1;

	if (ldata->icanon && !L_EXTPROC(tty))
		return ldata->canon_head != ldata->read_tail;
	else
		return ldata->commit_head - ldata->read_tail >= amt;
}

 
static bool copy_from_read_buf(const struct tty_struct *tty, u8 **kbp,
			       size_t *nr)

{
	struct n_tty_data *ldata = tty->disc_data;
	size_t n;
	bool is_eof;
	size_t head = smp_load_acquire(&ldata->commit_head);
	size_t tail = MASK(ldata->read_tail);

	n = min(head - ldata->read_tail, N_TTY_BUF_SIZE - tail);
	n = min(*nr, n);
	if (n) {
		u8 *from = read_buf_addr(ldata, tail);
		memcpy(*kbp, from, n);
		is_eof = n == 1 && *from == EOF_CHAR(tty);
		tty_audit_add_data(tty, from, n);
		zero_buffer(tty, from, n);
		smp_store_release(&ldata->read_tail, ldata->read_tail + n);
		 
		if (L_EXTPROC(tty) && ldata->icanon && is_eof &&
		    (head == ldata->read_tail))
			return false;
		*kbp += n;
		*nr -= n;

		 
		return head != ldata->read_tail;
	}
	return false;
}

 
static bool canon_copy_from_read_buf(const struct tty_struct *tty, u8 **kbp,
				     size_t *nr)
{
	struct n_tty_data *ldata = tty->disc_data;
	size_t n, size, more, c;
	size_t eol;
	size_t tail, canon_head;
	int found = 0;

	 
	if (!*nr)
		return false;

	canon_head = smp_load_acquire(&ldata->canon_head);
	n = min(*nr, canon_head - ldata->read_tail);

	tail = MASK(ldata->read_tail);
	size = min_t(size_t, tail + n, N_TTY_BUF_SIZE);

	n_tty_trace("%s: nr:%zu tail:%zu n:%zu size:%zu\n",
		    __func__, *nr, tail, n, size);

	eol = find_next_bit(ldata->read_flags, size, tail);
	more = n - (size - tail);
	if (eol == N_TTY_BUF_SIZE && more) {
		 
		eol = find_first_bit(ldata->read_flags, more);
		found = eol != more;
	} else
		found = eol != size;

	n = eol - tail;
	if (n > N_TTY_BUF_SIZE)
		n += N_TTY_BUF_SIZE;
	c = n + found;

	if (!found || read_buf(ldata, eol) != __DISABLED_CHAR)
		n = c;

	n_tty_trace("%s: eol:%zu found:%d n:%zu c:%zu tail:%zu more:%zu\n",
		    __func__, eol, found, n, c, tail, more);

	tty_copy(tty, *kbp, tail, n);
	*kbp += n;
	*nr -= n;

	if (found)
		clear_bit(eol, ldata->read_flags);
	smp_store_release(&ldata->read_tail, ldata->read_tail + c);

	if (found) {
		if (!ldata->push)
			ldata->line_start = ldata->read_tail;
		else
			ldata->push = 0;
		tty_audit_push();
		return false;
	}

	 
	return ldata->read_tail != canon_head;
}

 
static void canon_skip_eof(struct n_tty_data *ldata)
{
	size_t tail, canon_head;

	canon_head = smp_load_acquire(&ldata->canon_head);
	tail = ldata->read_tail;

	 
	if (tail == canon_head)
		return;

	 
	tail &= (N_TTY_BUF_SIZE - 1);
	if (!test_bit(tail, ldata->read_flags))
		return;
	if (read_buf(ldata, tail) != __DISABLED_CHAR)
		return;

	 
	clear_bit(tail, ldata->read_flags);
	smp_store_release(&ldata->read_tail, ldata->read_tail + 1);
}

 
static int job_control(struct tty_struct *tty, struct file *file)
{
	 
	 
	 
	if (file->f_op->write_iter == redirected_tty_write)
		return 0;

	return __tty_check_change(tty, SIGTTIN);
}


 
static ssize_t n_tty_read(struct tty_struct *tty, struct file *file, u8 *kbuf,
			  size_t nr, void **cookie, unsigned long offset)
{
	struct n_tty_data *ldata = tty->disc_data;
	u8 *kb = kbuf;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);
	int c;
	int minimum, time;
	ssize_t retval = 0;
	long timeout;
	bool packet;
	size_t old_tail;

	 
	if (*cookie) {
		if (ldata->icanon && !L_EXTPROC(tty)) {
			 
			if (!nr)
				canon_skip_eof(ldata);
			else if (canon_copy_from_read_buf(tty, &kb, &nr))
				return kb - kbuf;
		} else {
			if (copy_from_read_buf(tty, &kb, &nr))
				return kb - kbuf;
		}

		 
		n_tty_kick_worker(tty);
		n_tty_check_unthrottle(tty);
		up_read(&tty->termios_rwsem);
		mutex_unlock(&ldata->atomic_read_lock);
		*cookie = NULL;
		return kb - kbuf;
	}

	c = job_control(tty, file);
	if (c < 0)
		return c;

	 
	if (file->f_flags & O_NONBLOCK) {
		if (!mutex_trylock(&ldata->atomic_read_lock))
			return -EAGAIN;
	} else {
		if (mutex_lock_interruptible(&ldata->atomic_read_lock))
			return -ERESTARTSYS;
	}

	down_read(&tty->termios_rwsem);

	minimum = time = 0;
	timeout = MAX_SCHEDULE_TIMEOUT;
	if (!ldata->icanon) {
		minimum = MIN_CHAR(tty);
		if (minimum) {
			time = (HZ / 10) * TIME_CHAR(tty);
		} else {
			timeout = (HZ / 10) * TIME_CHAR(tty);
			minimum = 1;
		}
	}

	packet = tty->ctrl.packet;
	old_tail = ldata->read_tail;

	add_wait_queue(&tty->read_wait, &wait);
	while (nr) {
		 
		if (packet && tty->link->ctrl.pktstatus) {
			u8 cs;
			if (kb != kbuf)
				break;
			spin_lock_irq(&tty->link->ctrl.lock);
			cs = tty->link->ctrl.pktstatus;
			tty->link->ctrl.pktstatus = 0;
			spin_unlock_irq(&tty->link->ctrl.lock);
			*kb++ = cs;
			nr--;
			break;
		}

		if (!input_available_p(tty, 0)) {
			up_read(&tty->termios_rwsem);
			tty_buffer_flush_work(tty->port);
			down_read(&tty->termios_rwsem);
			if (!input_available_p(tty, 0)) {
				if (test_bit(TTY_OTHER_CLOSED, &tty->flags)) {
					retval = -EIO;
					break;
				}
				if (tty_hung_up_p(file))
					break;
				 
				if (test_bit(TTY_HUPPING, &tty->flags))
					break;
				if (!timeout)
					break;
				if (tty_io_nonblock(tty, file)) {
					retval = -EAGAIN;
					break;
				}
				if (signal_pending(current)) {
					retval = -ERESTARTSYS;
					break;
				}
				up_read(&tty->termios_rwsem);

				timeout = wait_woken(&wait, TASK_INTERRUPTIBLE,
						timeout);

				down_read(&tty->termios_rwsem);
				continue;
			}
		}

		if (ldata->icanon && !L_EXTPROC(tty)) {
			if (canon_copy_from_read_buf(tty, &kb, &nr))
				goto more_to_be_read;
		} else {
			 
			if (packet && kb == kbuf) {
				*kb++ = TIOCPKT_DATA;
				nr--;
			}

			 
			if (copy_from_read_buf(tty, &kb, &nr) && kb - kbuf >= minimum) {
more_to_be_read:
				remove_wait_queue(&tty->read_wait, &wait);
				*cookie = cookie;
				return kb - kbuf;
			}
		}

		n_tty_check_unthrottle(tty);

		if (kb - kbuf >= minimum)
			break;
		if (time)
			timeout = time;
	}
	if (old_tail != ldata->read_tail) {
		 
		smp_mb();
		n_tty_kick_worker(tty);
	}
	up_read(&tty->termios_rwsem);

	remove_wait_queue(&tty->read_wait, &wait);
	mutex_unlock(&ldata->atomic_read_lock);

	if (kb - kbuf)
		retval = kb - kbuf;

	return retval;
}

 

static ssize_t n_tty_write(struct tty_struct *tty, struct file *file,
			   const u8 *buf, size_t nr)
{
	const u8 *b = buf;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);
	ssize_t num, retval = 0;

	 
	if (L_TOSTOP(tty) && file->f_op->write_iter != redirected_tty_write) {
		retval = tty_check_change(tty);
		if (retval)
			return retval;
	}

	down_read(&tty->termios_rwsem);

	 
	process_echoes(tty);

	add_wait_queue(&tty->write_wait, &wait);
	while (1) {
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		if (tty_hung_up_p(file) || (tty->link && !tty->link->count)) {
			retval = -EIO;
			break;
		}
		if (O_OPOST(tty)) {
			while (nr > 0) {
				num = process_output_block(tty, b, nr);
				if (num < 0) {
					if (num == -EAGAIN)
						break;
					retval = num;
					goto break_out;
				}
				b += num;
				nr -= num;
				if (nr == 0)
					break;
				if (process_output(*b, tty) < 0)
					break;
				b++; nr--;
			}
			if (tty->ops->flush_chars)
				tty->ops->flush_chars(tty);
		} else {
			struct n_tty_data *ldata = tty->disc_data;

			while (nr > 0) {
				mutex_lock(&ldata->output_lock);
				num = tty->ops->write(tty, b, nr);
				mutex_unlock(&ldata->output_lock);
				if (num < 0) {
					retval = num;
					goto break_out;
				}
				if (!num)
					break;
				b += num;
				nr -= num;
			}
		}
		if (!nr)
			break;
		if (tty_io_nonblock(tty, file)) {
			retval = -EAGAIN;
			break;
		}
		up_read(&tty->termios_rwsem);

		wait_woken(&wait, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);

		down_read(&tty->termios_rwsem);
	}
break_out:
	remove_wait_queue(&tty->write_wait, &wait);
	if (nr && tty->fasync)
		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	up_read(&tty->termios_rwsem);
	return (b - buf) ? b - buf : retval;
}

 
static __poll_t n_tty_poll(struct tty_struct *tty, struct file *file,
							poll_table *wait)
{
	__poll_t mask = 0;

	poll_wait(file, &tty->read_wait, wait);
	poll_wait(file, &tty->write_wait, wait);
	if (input_available_p(tty, 1))
		mask |= EPOLLIN | EPOLLRDNORM;
	else {
		tty_buffer_flush_work(tty->port);
		if (input_available_p(tty, 1))
			mask |= EPOLLIN | EPOLLRDNORM;
	}
	if (tty->ctrl.packet && tty->link->ctrl.pktstatus)
		mask |= EPOLLPRI | EPOLLIN | EPOLLRDNORM;
	if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
		mask |= EPOLLHUP;
	if (tty_hung_up_p(file))
		mask |= EPOLLHUP;
	if (tty->ops->write && !tty_is_writelocked(tty) &&
			tty_chars_in_buffer(tty) < WAKEUP_CHARS &&
			tty_write_room(tty) > 0)
		mask |= EPOLLOUT | EPOLLWRNORM;
	return mask;
}

static unsigned long inq_canon(struct n_tty_data *ldata)
{
	size_t nr, head, tail;

	if (ldata->canon_head == ldata->read_tail)
		return 0;
	head = ldata->canon_head;
	tail = ldata->read_tail;
	nr = head - tail;
	 
	while (MASK(head) != MASK(tail)) {
		if (test_bit(MASK(tail), ldata->read_flags) &&
		    read_buf(ldata, tail) == __DISABLED_CHAR)
			nr--;
		tail++;
	}
	return nr;
}

static int n_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
		       unsigned long arg)
{
	struct n_tty_data *ldata = tty->disc_data;
	int retval;

	switch (cmd) {
	case TIOCOUTQ:
		return put_user(tty_chars_in_buffer(tty), (int __user *) arg);
	case TIOCINQ:
		down_write(&tty->termios_rwsem);
		if (L_ICANON(tty) && !L_EXTPROC(tty))
			retval = inq_canon(ldata);
		else
			retval = read_cnt(ldata);
		up_write(&tty->termios_rwsem);
		return put_user(retval, (unsigned int __user *) arg);
	default:
		return n_tty_ioctl_helper(tty, cmd, arg);
	}
}

static struct tty_ldisc_ops n_tty_ops = {
	.owner		 = THIS_MODULE,
	.num		 = N_TTY,
	.name            = "n_tty",
	.open            = n_tty_open,
	.close           = n_tty_close,
	.flush_buffer    = n_tty_flush_buffer,
	.read            = n_tty_read,
	.write           = n_tty_write,
	.ioctl           = n_tty_ioctl,
	.set_termios     = n_tty_set_termios,
	.poll            = n_tty_poll,
	.receive_buf     = n_tty_receive_buf,
	.write_wakeup    = n_tty_write_wakeup,
	.receive_buf2	 = n_tty_receive_buf2,
	.lookahead_buf	 = n_tty_lookahead_flow_ctrl,
};

 

void n_tty_inherit_ops(struct tty_ldisc_ops *ops)
{
	*ops = n_tty_ops;
	ops->owner = NULL;
}
EXPORT_SYMBOL_GPL(n_tty_inherit_ops);

void __init n_tty_init(void)
{
	tty_register_ldisc(&n_tty_ops);
}
