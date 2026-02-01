
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/kgdb.h>
#include <linux/kdb.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/vt_kern.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>

#define MAX_CONFIG_LEN		40

static struct kgdb_io		kgdboc_io_ops;

 
static int configured		= -1;
static DEFINE_MUTEX(config_mutex);

static char config[MAX_CONFIG_LEN];
static struct kparam_string kps = {
	.string			= config,
	.maxlen			= MAX_CONFIG_LEN,
};

static int kgdboc_use_kms;   
static struct tty_driver	*kgdb_tty_driver;
static int			kgdb_tty_line;

static struct platform_device *kgdboc_pdev;

#if IS_BUILTIN(CONFIG_KGDB_SERIAL_CONSOLE)
static struct kgdb_io		kgdboc_earlycon_io_ops;
static int                      (*earlycon_orig_exit)(struct console *con);
#endif  

#ifdef CONFIG_KDB_KEYBOARD
static int kgdboc_reset_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id)
{
	input_reset_device(dev);

	 
	return -ENODEV;
}

static void kgdboc_reset_disconnect(struct input_handle *handle)
{
	 
	BUG();
}

static const struct input_device_id kgdboc_reset_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ }
};

static struct input_handler kgdboc_reset_handler = {
	.connect	= kgdboc_reset_connect,
	.disconnect	= kgdboc_reset_disconnect,
	.name		= "kgdboc_reset",
	.id_table	= kgdboc_reset_ids,
};

static DEFINE_MUTEX(kgdboc_reset_mutex);

static void kgdboc_restore_input_helper(struct work_struct *dummy)
{
	 
	mutex_lock(&kgdboc_reset_mutex);

	if (input_register_handler(&kgdboc_reset_handler) == 0)
		input_unregister_handler(&kgdboc_reset_handler);

	mutex_unlock(&kgdboc_reset_mutex);
}

static DECLARE_WORK(kgdboc_restore_input_work, kgdboc_restore_input_helper);

static void kgdboc_restore_input(void)
{
	if (likely(system_state == SYSTEM_RUNNING))
		schedule_work(&kgdboc_restore_input_work);
}

static int kgdboc_register_kbd(char **cptr)
{
	if (strncmp(*cptr, "kbd", 3) == 0 ||
		strncmp(*cptr, "kdb", 3) == 0) {
		if (kdb_poll_idx < KDB_POLL_FUNC_MAX) {
			kdb_poll_funcs[kdb_poll_idx] = kdb_get_kbd_char;
			kdb_poll_idx++;
			if (cptr[0][3] == ',')
				*cptr += 4;
			else
				return 1;
		}
	}
	return 0;
}

static void kgdboc_unregister_kbd(void)
{
	int i;

	for (i = 0; i < kdb_poll_idx; i++) {
		if (kdb_poll_funcs[i] == kdb_get_kbd_char) {
			kdb_poll_idx--;
			kdb_poll_funcs[i] = kdb_poll_funcs[kdb_poll_idx];
			kdb_poll_funcs[kdb_poll_idx] = NULL;
			i--;
		}
	}
	flush_work(&kgdboc_restore_input_work);
}
#else  
#define kgdboc_register_kbd(x) 0
#define kgdboc_unregister_kbd()
#define kgdboc_restore_input()
#endif  

#if IS_BUILTIN(CONFIG_KGDB_SERIAL_CONSOLE)
static void cleanup_earlycon(void)
{
	if (kgdboc_earlycon_io_ops.cons)
		kgdb_unregister_io_module(&kgdboc_earlycon_io_ops);
}
#else  
static inline void cleanup_earlycon(void) { }
#endif  

static void cleanup_kgdboc(void)
{
	cleanup_earlycon();

	if (configured != 1)
		return;

	if (kgdb_unregister_nmi_console())
		return;
	kgdboc_unregister_kbd();
	kgdb_unregister_io_module(&kgdboc_io_ops);
}

static int configure_kgdboc(void)
{
	struct tty_driver *p;
	int tty_line = 0;
	int err = -ENODEV;
	char *cptr = config;
	struct console *cons;
	int cookie;

	if (!strlen(config) || isspace(config[0])) {
		err = 0;
		goto noconfig;
	}

	kgdboc_io_ops.cons = NULL;
	kgdb_tty_driver = NULL;

	kgdboc_use_kms = 0;
	if (strncmp(cptr, "kms,", 4) == 0) {
		cptr += 4;
		kgdboc_use_kms = 1;
	}

	if (kgdboc_register_kbd(&cptr))
		goto do_register;

	p = tty_find_polling_driver(cptr, &tty_line);
	if (!p)
		goto noconfig;

	 
	console_lock();

	cookie = console_srcu_read_lock();
	for_each_console_srcu(cons) {
		int idx;
		if (cons->device && cons->device(cons, &idx) == p &&
		    idx == tty_line) {
			kgdboc_io_ops.cons = cons;
			break;
		}
	}
	console_srcu_read_unlock(cookie);

	console_unlock();

	kgdb_tty_driver = p;
	kgdb_tty_line = tty_line;

do_register:
	err = kgdb_register_io_module(&kgdboc_io_ops);
	if (err)
		goto noconfig;

	err = kgdb_register_nmi_console();
	if (err)
		goto nmi_con_failed;

	configured = 1;

	return 0;

nmi_con_failed:
	kgdb_unregister_io_module(&kgdboc_io_ops);
noconfig:
	kgdboc_unregister_kbd();
	configured = 0;

	return err;
}

static int kgdboc_probe(struct platform_device *pdev)
{
	int ret = 0;

	mutex_lock(&config_mutex);
	if (configured != 1) {
		ret = configure_kgdboc();

		 
		if (ret == -ENODEV)
			ret = -EPROBE_DEFER;
	}
	mutex_unlock(&config_mutex);

	return ret;
}

static struct platform_driver kgdboc_platform_driver = {
	.probe = kgdboc_probe,
	.driver = {
		.name = "kgdboc",
		.suppress_bind_attrs = true,
	},
};

static int __init init_kgdboc(void)
{
	int ret;

	 
	ret = platform_driver_register(&kgdboc_platform_driver);
	if (ret)
		return ret;

	kgdboc_pdev = platform_device_alloc("kgdboc", PLATFORM_DEVID_NONE);
	if (!kgdboc_pdev) {
		ret = -ENOMEM;
		goto err_did_register;
	}

	ret = platform_device_add(kgdboc_pdev);
	if (!ret)
		return 0;

	platform_device_put(kgdboc_pdev);

err_did_register:
	platform_driver_unregister(&kgdboc_platform_driver);
	return ret;
}

static void exit_kgdboc(void)
{
	mutex_lock(&config_mutex);
	cleanup_kgdboc();
	mutex_unlock(&config_mutex);

	platform_device_unregister(kgdboc_pdev);
	platform_driver_unregister(&kgdboc_platform_driver);
}

static int kgdboc_get_char(void)
{
	if (!kgdb_tty_driver)
		return -1;
	return kgdb_tty_driver->ops->poll_get_char(kgdb_tty_driver,
						kgdb_tty_line);
}

static void kgdboc_put_char(u8 chr)
{
	if (!kgdb_tty_driver)
		return;
	kgdb_tty_driver->ops->poll_put_char(kgdb_tty_driver,
					kgdb_tty_line, chr);
}

static int param_set_kgdboc_var(const char *kmessage,
				const struct kernel_param *kp)
{
	size_t len = strlen(kmessage);
	int ret = 0;

	if (len >= MAX_CONFIG_LEN) {
		pr_err("config string too long\n");
		return -ENOSPC;
	}

	if (kgdb_connected) {
		pr_err("Cannot reconfigure while KGDB is connected.\n");
		return -EBUSY;
	}

	mutex_lock(&config_mutex);

	strcpy(config, kmessage);
	 
	if (len && config[len - 1] == '\n')
		config[len - 1] = '\0';

	if (configured == 1)
		cleanup_kgdboc();

	 
	if (configured >= 0)
		ret = configure_kgdboc();

	 
	if (ret)
		config[0] = '\0';

	mutex_unlock(&config_mutex);

	return ret;
}

static int dbg_restore_graphics;

static void kgdboc_pre_exp_handler(void)
{
	if (!dbg_restore_graphics && kgdboc_use_kms) {
		dbg_restore_graphics = 1;
		con_debug_enter(vc_cons[fg_console].d);
	}
	 
	if (!kgdb_connected)
		try_module_get(THIS_MODULE);
}

static void kgdboc_post_exp_handler(void)
{
	 
	if (!kgdb_connected)
		module_put(THIS_MODULE);
	if (kgdboc_use_kms && dbg_restore_graphics) {
		dbg_restore_graphics = 0;
		con_debug_leave();
	}
	kgdboc_restore_input();
}

static struct kgdb_io kgdboc_io_ops = {
	.name			= "kgdboc",
	.read_char		= kgdboc_get_char,
	.write_char		= kgdboc_put_char,
	.pre_exception		= kgdboc_pre_exp_handler,
	.post_exception		= kgdboc_post_exp_handler,
};

#if IS_BUILTIN(CONFIG_KGDB_SERIAL_CONSOLE)
static int kgdboc_option_setup(char *opt)
{
	if (!opt) {
		pr_err("config string not provided\n");
		return 1;
	}

	if (strlen(opt) >= MAX_CONFIG_LEN) {
		pr_err("config string too long\n");
		return 1;
	}
	strcpy(config, opt);

	return 1;
}

__setup("kgdboc=", kgdboc_option_setup);


 
static int __init kgdboc_early_init(char *opt)
{
	kgdboc_option_setup(opt);
	configure_kgdboc();
	return 0;
}

early_param("ekgdboc", kgdboc_early_init);

static int kgdboc_earlycon_get_char(void)
{
	char c;

	if (!kgdboc_earlycon_io_ops.cons->read(kgdboc_earlycon_io_ops.cons,
					       &c, 1))
		return NO_POLL_CHAR;

	return c;
}

static void kgdboc_earlycon_put_char(u8 chr)
{
	kgdboc_earlycon_io_ops.cons->write(kgdboc_earlycon_io_ops.cons, &chr,
					   1);
}

static void kgdboc_earlycon_pre_exp_handler(void)
{
	struct console *con;
	static bool already_warned;
	int cookie;

	if (already_warned)
		return;

	 
	cookie = console_srcu_read_lock();
	for_each_console_srcu(con) {
		if (con == kgdboc_earlycon_io_ops.cons)
			break;
	}
	console_srcu_read_unlock(cookie);
	if (con)
		return;

	already_warned = true;
	pr_warn("kgdboc_earlycon is still using bootconsole\n");
}

static int kgdboc_earlycon_deferred_exit(struct console *con)
{
	 
	con->exit = earlycon_orig_exit;

	return 0;
}

static void kgdboc_earlycon_deinit(void)
{
	if (!kgdboc_earlycon_io_ops.cons)
		return;

	if (kgdboc_earlycon_io_ops.cons->exit == kgdboc_earlycon_deferred_exit)
		 
		kgdboc_earlycon_io_ops.cons->exit = earlycon_orig_exit;
	else if (kgdboc_earlycon_io_ops.cons->exit)
		 
		kgdboc_earlycon_io_ops.cons->exit(kgdboc_earlycon_io_ops.cons);

	kgdboc_earlycon_io_ops.cons = NULL;
}

static struct kgdb_io kgdboc_earlycon_io_ops = {
	.name			= "kgdboc_earlycon",
	.read_char		= kgdboc_earlycon_get_char,
	.write_char		= kgdboc_earlycon_put_char,
	.pre_exception		= kgdboc_earlycon_pre_exp_handler,
	.deinit			= kgdboc_earlycon_deinit,
};

#define MAX_CONSOLE_NAME_LEN (sizeof((struct console *) 0)->name)
static char kgdboc_earlycon_param[MAX_CONSOLE_NAME_LEN] __initdata;
static bool kgdboc_earlycon_late_enable __initdata;

static int __init kgdboc_earlycon_init(char *opt)
{
	struct console *con;

	kdb_init(KDB_INIT_EARLY);

	 

	 
	console_list_lock();
	for_each_console(con) {
		if (con->write && con->read &&
		    (con->flags & (CON_BOOT | CON_ENABLED)) &&
		    (!opt || !opt[0] || strcmp(con->name, opt) == 0))
			break;
	}

	if (!con) {
		 
		if (!kgdboc_earlycon_late_enable) {
			pr_info("No suitable earlycon yet, will try later\n");
			if (opt)
				strscpy(kgdboc_earlycon_param, opt,
					sizeof(kgdboc_earlycon_param));
			kgdboc_earlycon_late_enable = true;
		} else {
			pr_info("Couldn't find kgdb earlycon\n");
		}
		goto unlock;
	}

	kgdboc_earlycon_io_ops.cons = con;
	pr_info("Going to register kgdb with earlycon '%s'\n", con->name);
	if (kgdb_register_io_module(&kgdboc_earlycon_io_ops) != 0) {
		kgdboc_earlycon_io_ops.cons = NULL;
		pr_info("Failed to register kgdb with earlycon\n");
	} else {
		 
		earlycon_orig_exit = con->exit;
		con->exit = kgdboc_earlycon_deferred_exit;
	}

unlock:
	console_list_unlock();

	 
	return 0;
}

early_param("kgdboc_earlycon", kgdboc_earlycon_init);

 
static int __init kgdboc_earlycon_late_init(void)
{
	if (kgdboc_earlycon_late_enable)
		kgdboc_earlycon_init(kgdboc_earlycon_param);
	return 0;
}
console_initcall(kgdboc_earlycon_late_init);

#endif  

module_init(init_kgdboc);
module_exit(exit_kgdboc);
module_param_call(kgdboc, param_set_kgdboc_var, param_get_string, &kps, 0644);
MODULE_PARM_DESC(kgdboc, "<serial_device>[,baud]");
MODULE_DESCRIPTION("KGDB Console TTY Driver");
MODULE_LICENSE("GPL");
