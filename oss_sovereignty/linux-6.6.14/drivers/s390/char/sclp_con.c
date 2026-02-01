
 

#include <linux/kmod.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/panic_notifier.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/termios.h>
#include <linux/err.h>
#include <linux/reboot.h>
#include <linux/gfp.h>

#include "sclp.h"
#include "sclp_rw.h"
#include "sclp_tty.h"

#define sclp_console_major 4		 
#define sclp_console_minor 64
#define sclp_console_name  "ttyS"

 
static DEFINE_SPINLOCK(sclp_con_lock);
 
static LIST_HEAD(sclp_con_pages);
 
static LIST_HEAD(sclp_con_outqueue);
 
static struct sclp_buffer *sclp_conbuf;
 
static struct timer_list sclp_con_timer;
 
static int sclp_con_queue_running;

 
#define SCLP_CON_COLUMNS	320
#define SPACES_PER_TAB		8

static void
sclp_conbuf_callback(struct sclp_buffer *buffer, int rc)
{
	unsigned long flags;
	void *page;

	do {
		page = sclp_unmake_buffer(buffer);
		spin_lock_irqsave(&sclp_con_lock, flags);

		 
		list_del(&buffer->list);
		list_add_tail((struct list_head *) page, &sclp_con_pages);

		 
		buffer = NULL;
		if (!list_empty(&sclp_con_outqueue))
			buffer = list_first_entry(&sclp_con_outqueue,
						  struct sclp_buffer, list);
		if (!buffer) {
			sclp_con_queue_running = 0;
			spin_unlock_irqrestore(&sclp_con_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&sclp_con_lock, flags);
	} while (sclp_emit_buffer(buffer, sclp_conbuf_callback));
}

 
static void sclp_conbuf_emit(void)
{
	struct sclp_buffer* buffer;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&sclp_con_lock, flags);
	if (sclp_conbuf)
		list_add_tail(&sclp_conbuf->list, &sclp_con_outqueue);
	sclp_conbuf = NULL;
	if (sclp_con_queue_running)
		goto out_unlock;
	if (list_empty(&sclp_con_outqueue))
		goto out_unlock;
	buffer = list_first_entry(&sclp_con_outqueue, struct sclp_buffer,
				  list);
	sclp_con_queue_running = 1;
	spin_unlock_irqrestore(&sclp_con_lock, flags);

	rc = sclp_emit_buffer(buffer, sclp_conbuf_callback);
	if (rc)
		sclp_conbuf_callback(buffer, rc);
	return;
out_unlock:
	spin_unlock_irqrestore(&sclp_con_lock, flags);
}

 
static void sclp_console_sync_queue(void)
{
	unsigned long flags;

	spin_lock_irqsave(&sclp_con_lock, flags);
	del_timer(&sclp_con_timer);
	while (sclp_con_queue_running) {
		spin_unlock_irqrestore(&sclp_con_lock, flags);
		sclp_sync_wait();
		spin_lock_irqsave(&sclp_con_lock, flags);
	}
	spin_unlock_irqrestore(&sclp_con_lock, flags);
}

 
static void
sclp_console_timeout(struct timer_list *unused)
{
	sclp_conbuf_emit();
}

 
static int
sclp_console_drop_buffer(void)
{
	struct list_head *list;
	struct sclp_buffer *buffer;
	void *page;

	if (!sclp_console_drop)
		return 0;
	list = sclp_con_outqueue.next;
	if (sclp_con_queue_running)
		 
		list = list->next;
	if (list == &sclp_con_outqueue)
		return 0;
	list_del(list);
	buffer = list_entry(list, struct sclp_buffer, list);
	page = sclp_unmake_buffer(buffer);
	list_add_tail((struct list_head *) page, &sclp_con_pages);
	return 1;
}

 
static void
sclp_console_write(struct console *console, const char *message,
		   unsigned int count)
{
	unsigned long flags;
	void *page;
	int written;

	if (count == 0)
		return;
	spin_lock_irqsave(&sclp_con_lock, flags);
	 
	do {
		 
		if (sclp_conbuf == NULL) {
			if (list_empty(&sclp_con_pages))
				sclp_console_full++;
			while (list_empty(&sclp_con_pages)) {
				if (sclp_console_drop_buffer())
					break;
				spin_unlock_irqrestore(&sclp_con_lock, flags);
				sclp_sync_wait();
				spin_lock_irqsave(&sclp_con_lock, flags);
			}
			page = sclp_con_pages.next;
			list_del((struct list_head *) page);
			sclp_conbuf = sclp_make_buffer(page, SCLP_CON_COLUMNS,
						       SPACES_PER_TAB);
		}
		 
		written = sclp_write(sclp_conbuf, (const unsigned char *)
				     message, count);
		if (written == count)
			break;
		 
		spin_unlock_irqrestore(&sclp_con_lock, flags);
		sclp_conbuf_emit();
		spin_lock_irqsave(&sclp_con_lock, flags);
		message += written;
		count -= written;
	} while (count > 0);
	 
	if (sclp_conbuf != NULL && sclp_chars_in_buffer(sclp_conbuf) != 0 &&
	    !timer_pending(&sclp_con_timer)) {
		mod_timer(&sclp_con_timer, jiffies + HZ / 10);
	}
	spin_unlock_irqrestore(&sclp_con_lock, flags);
}

static struct tty_driver *
sclp_console_device(struct console *c, int *index)
{
	*index = c->index;
	return sclp_tty_driver;
}

 
static int sclp_console_notify(struct notifier_block *self,
			       unsigned long event, void *data)
{
	 
	if (spin_is_locked(&sclp_con_lock))
		return NOTIFY_DONE;

	sclp_conbuf_emit();
	sclp_console_sync_queue();

	return NOTIFY_DONE;
}

static struct notifier_block on_panic_nb = {
	.notifier_call = sclp_console_notify,
	.priority = INT_MIN + 1,  
};

static struct notifier_block on_reboot_nb = {
	.notifier_call = sclp_console_notify,
	.priority = INT_MIN + 1,  
};

 
static struct console sclp_console =
{
	.name = sclp_console_name,
	.write = sclp_console_write,
	.device = sclp_console_device,
	.flags = CON_PRINTBUFFER,
	.index = 0  
};

 
static int __init
sclp_console_init(void)
{
	void *page;
	int i;
	int rc;

	 
	if (!(CONSOLE_IS_SCLP || CONSOLE_IS_VT220))
		return 0;
	rc = sclp_rw_init();
	if (rc)
		return rc;
	 
	for (i = 0; i < sclp_console_pages; i++) {
		page = (void *) get_zeroed_page(GFP_KERNEL | GFP_DMA);
		list_add_tail(page, &sclp_con_pages);
	}
	sclp_conbuf = NULL;
	timer_setup(&sclp_con_timer, sclp_console_timeout, 0);

	 
	atomic_notifier_chain_register(&panic_notifier_list, &on_panic_nb);
	register_reboot_notifier(&on_reboot_nb);
	register_console(&sclp_console);
	return 0;
}

console_initcall(sclp_console_init);
