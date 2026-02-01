
 

#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include "internal.h"

#ifdef CONFIG_MODULE_UNLOAD
static inline void print_unload_info(struct seq_file *m, struct module *mod)
{
	struct module_use *use;
	int printed_something = 0;

	seq_printf(m, " %i ", module_refcount(mod));

	 
	list_for_each_entry(use, &mod->source_list, source_list) {
		printed_something = 1;
		seq_printf(m, "%s,", use->source->name);
	}

	if (mod->init && !mod->exit) {
		printed_something = 1;
		seq_puts(m, "[permanent],");
	}

	if (!printed_something)
		seq_puts(m, "-");
}
#else  
static inline void print_unload_info(struct seq_file *m, struct module *mod)
{
	 
	seq_puts(m, " - -");
}
#endif  

 
static void *m_start(struct seq_file *m, loff_t *pos)
{
	mutex_lock(&module_mutex);
	return seq_list_start(&modules, *pos);
}

static void *m_next(struct seq_file *m, void *p, loff_t *pos)
{
	return seq_list_next(p, &modules, pos);
}

static void m_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&module_mutex);
}

static unsigned int module_total_size(struct module *mod)
{
	int size = 0;

	for_each_mod_mem_type(type)
		size += mod->mem[type].size;
	return size;
}

static int m_show(struct seq_file *m, void *p)
{
	struct module *mod = list_entry(p, struct module, list);
	char buf[MODULE_FLAGS_BUF_SIZE];
	void *value;
	unsigned int size;

	 
	if (mod->state == MODULE_STATE_UNFORMED)
		return 0;

	size = module_total_size(mod);
	seq_printf(m, "%s %u", mod->name, size);
	print_unload_info(m, mod);

	 
	seq_printf(m, " %s",
		   mod->state == MODULE_STATE_GOING ? "Unloading" :
		   mod->state == MODULE_STATE_COMING ? "Loading" :
		   "Live");
	 
	value = m->private ? NULL : mod->mem[MOD_TEXT].base;
	seq_printf(m, " 0x%px", value);

	 
	if (mod->taints)
		seq_printf(m, " %s", module_flags(mod, buf, true));

	seq_puts(m, "\n");
	return 0;
}

 
static const struct seq_operations modules_op = {
	.start	= m_start,
	.next	= m_next,
	.stop	= m_stop,
	.show	= m_show
};

 
static int modules_open(struct inode *inode, struct file *file)
{
	int err = seq_open(file, &modules_op);

	if (!err) {
		struct seq_file *m = file->private_data;

		m->private = kallsyms_show_value(file->f_cred) ? NULL : (void *)8ul;
	}

	return err;
}

static const struct proc_ops modules_proc_ops = {
	.proc_flags	= PROC_ENTRY_PERMANENT,
	.proc_open	= modules_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release,
};

static int __init proc_modules_init(void)
{
	proc_create("modules", 0, NULL, &modules_proc_ops);
	return 0;
}
module_init(proc_modules_init);
