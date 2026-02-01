
 

 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/console.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/netpoll.h>
#include <linux/inet.h>
#include <linux/configfs.h>
#include <linux/etherdevice.h>
#include <linux/utsname.h>

MODULE_AUTHOR("Maintainer: Matt Mackall <mpm@selenic.com>");
MODULE_DESCRIPTION("Console driver for network interfaces");
MODULE_LICENSE("GPL");

#define MAX_PARAM_LENGTH	256
#define MAX_PRINT_CHUNK		1000

static char config[MAX_PARAM_LENGTH];
module_param_string(netconsole, config, MAX_PARAM_LENGTH, 0);
MODULE_PARM_DESC(netconsole, " netconsole=[src-port]@[src-ip]/[dev],[tgt-port]@<tgt-ip>/[tgt-macaddr]");

static bool oops_only = false;
module_param(oops_only, bool, 0600);
MODULE_PARM_DESC(oops_only, "Only log oops messages");

#ifndef	MODULE
static int __init option_setup(char *opt)
{
	strscpy(config, opt, MAX_PARAM_LENGTH);
	return 1;
}
__setup("netconsole=", option_setup);
#endif	 

 
static LIST_HEAD(target_list);

 
static DEFINE_SPINLOCK(target_list_lock);

 
static struct console netconsole_ext;

 
struct netconsole_target {
	struct list_head	list;
#ifdef	CONFIG_NETCONSOLE_DYNAMIC
	struct config_item	item;
#endif
	bool			enabled;
	bool			extended;
	bool			release;
	struct netpoll		np;
};

#ifdef	CONFIG_NETCONSOLE_DYNAMIC

static struct configfs_subsystem netconsole_subsys;
static DEFINE_MUTEX(dynamic_netconsole_mutex);

static int __init dynamic_netconsole_init(void)
{
	config_group_init(&netconsole_subsys.su_group);
	mutex_init(&netconsole_subsys.su_mutex);
	return configfs_register_subsystem(&netconsole_subsys);
}

static void __exit dynamic_netconsole_exit(void)
{
	configfs_unregister_subsystem(&netconsole_subsys);
}

 
static void netconsole_target_get(struct netconsole_target *nt)
{
	if (config_item_name(&nt->item))
		config_item_get(&nt->item);
}

static void netconsole_target_put(struct netconsole_target *nt)
{
	if (config_item_name(&nt->item))
		config_item_put(&nt->item);
}

#else	 

static int __init dynamic_netconsole_init(void)
{
	return 0;
}

static void __exit dynamic_netconsole_exit(void)
{
}

 
static void netconsole_target_get(struct netconsole_target *nt)
{
}

static void netconsole_target_put(struct netconsole_target *nt)
{
}

#endif	 

 
static struct netconsole_target *alloc_and_init(void)
{
	struct netconsole_target *nt;

	nt = kzalloc(sizeof(*nt), GFP_KERNEL);
	if (!nt)
		return nt;

	if (IS_ENABLED(CONFIG_NETCONSOLE_EXTENDED_LOG))
		nt->extended = true;
	if (IS_ENABLED(CONFIG_NETCONSOLE_PREPEND_RELEASE))
		nt->release = true;

	nt->np.name = "netconsole";
	strscpy(nt->np.dev_name, "eth0", IFNAMSIZ);
	nt->np.local_port = 6665;
	nt->np.remote_port = 6666;
	eth_broadcast_addr(nt->np.remote_mac);

	return nt;
}

 
static struct netconsole_target *alloc_param_target(char *target_config)
{
	struct netconsole_target *nt;
	int err;

	nt = alloc_and_init();
	if (!nt) {
		err = -ENOMEM;
		goto fail;
	}

	if (*target_config == '+') {
		nt->extended = true;
		target_config++;
	}

	if (*target_config == 'r') {
		if (!nt->extended) {
			pr_err("Netconsole configuration error. Release feature requires extended log message");
			err = -EINVAL;
			goto fail;
		}
		nt->release = true;
		target_config++;
	}

	 
	err = netpoll_parse_options(&nt->np, target_config);
	if (err)
		goto fail;

	err = netpoll_setup(&nt->np);
	if (err)
		goto fail;

	nt->enabled = true;

	return nt;

fail:
	kfree(nt);
	return ERR_PTR(err);
}

 
static void free_param_target(struct netconsole_target *nt)
{
	netpoll_cleanup(&nt->np);
	kfree(nt);
}

#ifdef	CONFIG_NETCONSOLE_DYNAMIC

 

static struct netconsole_target *to_target(struct config_item *item)
{
	return item ?
		container_of(item, struct netconsole_target, item) :
		NULL;
}

 

static ssize_t enabled_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "%d\n", to_target(item)->enabled);
}

static ssize_t extended_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "%d\n", to_target(item)->extended);
}

static ssize_t release_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "%d\n", to_target(item)->release);
}

static ssize_t dev_name_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "%s\n", to_target(item)->np.dev_name);
}

static ssize_t local_port_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "%d\n", to_target(item)->np.local_port);
}

static ssize_t remote_port_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "%d\n", to_target(item)->np.remote_port);
}

static ssize_t local_ip_show(struct config_item *item, char *buf)
{
	struct netconsole_target *nt = to_target(item);

	if (nt->np.ipv6)
		return sysfs_emit(buf, "%pI6c\n", &nt->np.local_ip.in6);
	else
		return sysfs_emit(buf, "%pI4\n", &nt->np.local_ip);
}

static ssize_t remote_ip_show(struct config_item *item, char *buf)
{
	struct netconsole_target *nt = to_target(item);

	if (nt->np.ipv6)
		return sysfs_emit(buf, "%pI6c\n", &nt->np.remote_ip.in6);
	else
		return sysfs_emit(buf, "%pI4\n", &nt->np.remote_ip);
}

static ssize_t local_mac_show(struct config_item *item, char *buf)
{
	struct net_device *dev = to_target(item)->np.dev;
	static const u8 bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	return sysfs_emit(buf, "%pM\n", dev ? dev->dev_addr : bcast);
}

static ssize_t remote_mac_show(struct config_item *item, char *buf)
{
	return sysfs_emit(buf, "%pM\n", to_target(item)->np.remote_mac);
}

 
static ssize_t enabled_store(struct config_item *item,
		const char *buf, size_t count)
{
	struct netconsole_target *nt = to_target(item);
	unsigned long flags;
	bool enabled;
	int err;

	mutex_lock(&dynamic_netconsole_mutex);
	err = kstrtobool(buf, &enabled);
	if (err)
		goto out_unlock;

	err = -EINVAL;
	if ((bool)enabled == nt->enabled) {
		pr_info("network logging has already %s\n",
			nt->enabled ? "started" : "stopped");
		goto out_unlock;
	}

	if (enabled) {	 
		if (nt->release && !nt->extended) {
			pr_err("Not enabling netconsole. Release feature requires extended log message");
			goto out_unlock;
		}

		if (nt->extended && !console_is_registered(&netconsole_ext))
			register_console(&netconsole_ext);

		 
		netpoll_print_options(&nt->np);

		err = netpoll_setup(&nt->np);
		if (err)
			goto out_unlock;

		pr_info("network logging started\n");
	} else {	 
		 
		spin_lock_irqsave(&target_list_lock, flags);
		nt->enabled = false;
		spin_unlock_irqrestore(&target_list_lock, flags);
		netpoll_cleanup(&nt->np);
	}

	nt->enabled = enabled;

	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return err;
}

static ssize_t release_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct netconsole_target *nt = to_target(item);
	bool release;
	int err;

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		err = -EINVAL;
		goto out_unlock;
	}

	err = kstrtobool(buf, &release);
	if (err)
		goto out_unlock;

	nt->release = release;

	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return err;
}

static ssize_t extended_store(struct config_item *item, const char *buf,
		size_t count)
{
	struct netconsole_target *nt = to_target(item);
	bool extended;
	int err;

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		err = -EINVAL;
		goto out_unlock;
	}

	err = kstrtobool(buf, &extended);
	if (err)
		goto out_unlock;

	nt->extended = extended;

	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return err;
}

static ssize_t dev_name_store(struct config_item *item, const char *buf,
		size_t count)
{
	struct netconsole_target *nt = to_target(item);
	size_t len;

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		mutex_unlock(&dynamic_netconsole_mutex);
		return -EINVAL;
	}

	strscpy(nt->np.dev_name, buf, IFNAMSIZ);

	 
	len = strnlen(nt->np.dev_name, IFNAMSIZ);
	if (nt->np.dev_name[len - 1] == '\n')
		nt->np.dev_name[len - 1] = '\0';

	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
}

static ssize_t local_port_store(struct config_item *item, const char *buf,
		size_t count)
{
	struct netconsole_target *nt = to_target(item);
	int rv = -EINVAL;

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		goto out_unlock;
	}

	rv = kstrtou16(buf, 10, &nt->np.local_port);
	if (rv < 0)
		goto out_unlock;
	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return rv;
}

static ssize_t remote_port_store(struct config_item *item,
		const char *buf, size_t count)
{
	struct netconsole_target *nt = to_target(item);
	int rv = -EINVAL;

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		goto out_unlock;
	}

	rv = kstrtou16(buf, 10, &nt->np.remote_port);
	if (rv < 0)
		goto out_unlock;
	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return rv;
}

static ssize_t local_ip_store(struct config_item *item, const char *buf,
		size_t count)
{
	struct netconsole_target *nt = to_target(item);

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		goto out_unlock;
	}

	if (strnchr(buf, count, ':')) {
		const char *end;
		if (in6_pton(buf, count, nt->np.local_ip.in6.s6_addr, -1, &end) > 0) {
			if (*end && *end != '\n') {
				pr_err("invalid IPv6 address at: <%c>\n", *end);
				goto out_unlock;
			}
			nt->np.ipv6 = true;
		} else
			goto out_unlock;
	} else {
		if (!nt->np.ipv6) {
			nt->np.local_ip.ip = in_aton(buf);
		} else
			goto out_unlock;
	}

	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return -EINVAL;
}

static ssize_t remote_ip_store(struct config_item *item, const char *buf,
	       size_t count)
{
	struct netconsole_target *nt = to_target(item);

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		goto out_unlock;
	}

	if (strnchr(buf, count, ':')) {
		const char *end;
		if (in6_pton(buf, count, nt->np.remote_ip.in6.s6_addr, -1, &end) > 0) {
			if (*end && *end != '\n') {
				pr_err("invalid IPv6 address at: <%c>\n", *end);
				goto out_unlock;
			}
			nt->np.ipv6 = true;
		} else
			goto out_unlock;
	} else {
		if (!nt->np.ipv6) {
			nt->np.remote_ip.ip = in_aton(buf);
		} else
			goto out_unlock;
	}

	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return -EINVAL;
}

static ssize_t remote_mac_store(struct config_item *item, const char *buf,
		size_t count)
{
	struct netconsole_target *nt = to_target(item);
	u8 remote_mac[ETH_ALEN];

	mutex_lock(&dynamic_netconsole_mutex);
	if (nt->enabled) {
		pr_err("target (%s) is enabled, disable to update parameters\n",
		       config_item_name(&nt->item));
		goto out_unlock;
	}

	if (!mac_pton(buf, remote_mac))
		goto out_unlock;
	if (buf[3 * ETH_ALEN - 1] && buf[3 * ETH_ALEN - 1] != '\n')
		goto out_unlock;
	memcpy(nt->np.remote_mac, remote_mac, ETH_ALEN);

	mutex_unlock(&dynamic_netconsole_mutex);
	return strnlen(buf, count);
out_unlock:
	mutex_unlock(&dynamic_netconsole_mutex);
	return -EINVAL;
}

CONFIGFS_ATTR(, enabled);
CONFIGFS_ATTR(, extended);
CONFIGFS_ATTR(, dev_name);
CONFIGFS_ATTR(, local_port);
CONFIGFS_ATTR(, remote_port);
CONFIGFS_ATTR(, local_ip);
CONFIGFS_ATTR(, remote_ip);
CONFIGFS_ATTR_RO(, local_mac);
CONFIGFS_ATTR(, remote_mac);
CONFIGFS_ATTR(, release);

static struct configfs_attribute *netconsole_target_attrs[] = {
	&attr_enabled,
	&attr_extended,
	&attr_release,
	&attr_dev_name,
	&attr_local_port,
	&attr_remote_port,
	&attr_local_ip,
	&attr_remote_ip,
	&attr_local_mac,
	&attr_remote_mac,
	NULL,
};

 

static void netconsole_target_release(struct config_item *item)
{
	kfree(to_target(item));
}

static struct configfs_item_operations netconsole_target_item_ops = {
	.release		= netconsole_target_release,
};

static const struct config_item_type netconsole_target_type = {
	.ct_attrs		= netconsole_target_attrs,
	.ct_item_ops		= &netconsole_target_item_ops,
	.ct_owner		= THIS_MODULE,
};

 

static struct config_item *make_netconsole_target(struct config_group *group,
						  const char *name)
{
	struct netconsole_target *nt;
	unsigned long flags;

	nt = alloc_and_init();
	if (!nt)
		return ERR_PTR(-ENOMEM);

	 
	config_item_init_type_name(&nt->item, name, &netconsole_target_type);

	 
	spin_lock_irqsave(&target_list_lock, flags);
	list_add(&nt->list, &target_list);
	spin_unlock_irqrestore(&target_list_lock, flags);

	return &nt->item;
}

static void drop_netconsole_target(struct config_group *group,
				   struct config_item *item)
{
	unsigned long flags;
	struct netconsole_target *nt = to_target(item);

	spin_lock_irqsave(&target_list_lock, flags);
	list_del(&nt->list);
	spin_unlock_irqrestore(&target_list_lock, flags);

	 
	if (nt->enabled)
		netpoll_cleanup(&nt->np);

	config_item_put(&nt->item);
}

static struct configfs_group_operations netconsole_subsys_group_ops = {
	.make_item	= make_netconsole_target,
	.drop_item	= drop_netconsole_target,
};

static const struct config_item_type netconsole_subsys_type = {
	.ct_group_ops	= &netconsole_subsys_group_ops,
	.ct_owner	= THIS_MODULE,
};

 
static struct configfs_subsystem netconsole_subsys = {
	.su_group	= {
		.cg_item	= {
			.ci_namebuf	= "netconsole",
			.ci_type	= &netconsole_subsys_type,
		},
	},
};

#endif	 

 
static int netconsole_netdev_event(struct notifier_block *this,
				   unsigned long event, void *ptr)
{
	unsigned long flags;
	struct netconsole_target *nt;
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	bool stopped = false;

	if (!(event == NETDEV_CHANGENAME || event == NETDEV_UNREGISTER ||
	      event == NETDEV_RELEASE || event == NETDEV_JOIN))
		goto done;

	spin_lock_irqsave(&target_list_lock, flags);
restart:
	list_for_each_entry(nt, &target_list, list) {
		netconsole_target_get(nt);
		if (nt->np.dev == dev) {
			switch (event) {
			case NETDEV_CHANGENAME:
				strscpy(nt->np.dev_name, dev->name, IFNAMSIZ);
				break;
			case NETDEV_RELEASE:
			case NETDEV_JOIN:
			case NETDEV_UNREGISTER:
				 
				spin_unlock_irqrestore(&target_list_lock, flags);

				__netpoll_cleanup(&nt->np);

				spin_lock_irqsave(&target_list_lock, flags);
				netdev_put(nt->np.dev, &nt->np.dev_tracker);
				nt->np.dev = NULL;
				nt->enabled = false;
				stopped = true;
				netconsole_target_put(nt);
				goto restart;
			}
		}
		netconsole_target_put(nt);
	}
	spin_unlock_irqrestore(&target_list_lock, flags);
	if (stopped) {
		const char *msg = "had an event";
		switch (event) {
		case NETDEV_UNREGISTER:
			msg = "unregistered";
			break;
		case NETDEV_RELEASE:
			msg = "released slaves";
			break;
		case NETDEV_JOIN:
			msg = "is joining a master device";
			break;
		}
		pr_info("network logging stopped on interface %s as it %s\n",
			dev->name, msg);
	}

done:
	return NOTIFY_DONE;
}

static struct notifier_block netconsole_netdev_notifier = {
	.notifier_call  = netconsole_netdev_event,
};

 
static void send_ext_msg_udp(struct netconsole_target *nt, const char *msg,
			     int msg_len)
{
	static char buf[MAX_PRINT_CHUNK];  
	const char *header, *body;
	int offset = 0;
	int header_len, body_len;
	const char *msg_ready = msg;
	const char *release;
	int release_len = 0;

	if (nt->release) {
		release = init_utsname()->release;
		release_len = strlen(release) + 1;
	}

	if (msg_len + release_len <= MAX_PRINT_CHUNK) {
		 
		if (nt->release) {
			scnprintf(buf, MAX_PRINT_CHUNK, "%s,%s", release, msg);
			msg_len += release_len;
			msg_ready = buf;
		}
		netpoll_send_udp(&nt->np, msg_ready, msg_len);
		return;
	}

	 
	header = msg;
	body = memchr(msg, ';', msg_len);
	if (WARN_ON_ONCE(!body))
		return;

	header_len = body - header;
	body_len = msg_len - header_len - 1;
	body++;

	 
	if (nt->release)
		scnprintf(buf, MAX_PRINT_CHUNK, "%s,", release);
	memcpy(buf + release_len, header, header_len);
	header_len += release_len;

	while (offset < body_len) {
		int this_header = header_len;
		int this_chunk;

		this_header += scnprintf(buf + this_header,
					 sizeof(buf) - this_header,
					 ",ncfrag=%d/%d;", offset, body_len);

		this_chunk = min(body_len - offset,
				 MAX_PRINT_CHUNK - this_header);
		if (WARN_ON_ONCE(this_chunk <= 0))
			return;

		memcpy(buf + this_header, body + offset, this_chunk);

		netpoll_send_udp(&nt->np, buf, this_header + this_chunk);

		offset += this_chunk;
	}
}

static void write_ext_msg(struct console *con, const char *msg,
			  unsigned int len)
{
	struct netconsole_target *nt;
	unsigned long flags;

	if ((oops_only && !oops_in_progress) || list_empty(&target_list))
		return;

	spin_lock_irqsave(&target_list_lock, flags);
	list_for_each_entry(nt, &target_list, list)
		if (nt->extended && nt->enabled && netif_running(nt->np.dev))
			send_ext_msg_udp(nt, msg, len);
	spin_unlock_irqrestore(&target_list_lock, flags);
}

static void write_msg(struct console *con, const char *msg, unsigned int len)
{
	int frag, left;
	unsigned long flags;
	struct netconsole_target *nt;
	const char *tmp;

	if (oops_only && !oops_in_progress)
		return;
	 
	if (list_empty(&target_list))
		return;

	spin_lock_irqsave(&target_list_lock, flags);
	list_for_each_entry(nt, &target_list, list) {
		if (!nt->extended && nt->enabled && netif_running(nt->np.dev)) {
			 
			tmp = msg;
			for (left = len; left;) {
				frag = min(left, MAX_PRINT_CHUNK);
				netpoll_send_udp(&nt->np, tmp, frag);
				tmp += frag;
				left -= frag;
			}
		}
	}
	spin_unlock_irqrestore(&target_list_lock, flags);
}

static struct console netconsole_ext = {
	.name	= "netcon_ext",
	.flags	= CON_ENABLED | CON_EXTENDED,
	.write	= write_ext_msg,
};

static struct console netconsole = {
	.name	= "netcon",
	.flags	= CON_ENABLED,
	.write	= write_msg,
};

static int __init init_netconsole(void)
{
	int err;
	struct netconsole_target *nt, *tmp;
	bool extended = false;
	unsigned long flags;
	char *target_config;
	char *input = config;

	if (strnlen(input, MAX_PARAM_LENGTH)) {
		while ((target_config = strsep(&input, ";"))) {
			nt = alloc_param_target(target_config);
			if (IS_ERR(nt)) {
				err = PTR_ERR(nt);
				goto fail;
			}
			 
			if (nt->extended) {
				extended = true;
				netconsole_ext.flags |= CON_PRINTBUFFER;
			} else {
				netconsole.flags |= CON_PRINTBUFFER;
			}

			spin_lock_irqsave(&target_list_lock, flags);
			list_add(&nt->list, &target_list);
			spin_unlock_irqrestore(&target_list_lock, flags);
		}
	}

	err = register_netdevice_notifier(&netconsole_netdev_notifier);
	if (err)
		goto fail;

	err = dynamic_netconsole_init();
	if (err)
		goto undonotifier;

	if (extended)
		register_console(&netconsole_ext);
	register_console(&netconsole);
	pr_info("network logging started\n");

	return err;

undonotifier:
	unregister_netdevice_notifier(&netconsole_netdev_notifier);

fail:
	pr_err("cleaning up\n");

	 
	list_for_each_entry_safe(nt, tmp, &target_list, list) {
		list_del(&nt->list);
		free_param_target(nt);
	}

	return err;
}

static void __exit cleanup_netconsole(void)
{
	struct netconsole_target *nt, *tmp;

	if (console_is_registered(&netconsole_ext))
		unregister_console(&netconsole_ext);
	unregister_console(&netconsole);
	dynamic_netconsole_exit();
	unregister_netdevice_notifier(&netconsole_netdev_notifier);

	 
	list_for_each_entry_safe(nt, tmp, &target_list, list) {
		list_del(&nt->list);
		free_param_target(nt);
	}
}

 
late_initcall(init_netconsole);
module_exit(cleanup_netconsole);
