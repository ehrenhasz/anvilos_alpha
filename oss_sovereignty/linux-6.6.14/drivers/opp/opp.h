#ifndef __DRIVER_OPP_H__
#define __DRIVER_OPP_H__
#include <linux/device.h>
#include <linux/interconnect.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/limits.h>
#include <linux/pm_opp.h>
#include <linux/notifier.h>
struct clk;
struct regulator;
extern struct mutex opp_table_lock;
extern struct list_head opp_tables;
#define OPP_CONFIG_CLK			BIT(0)
#define OPP_CONFIG_REGULATOR		BIT(1)
#define OPP_CONFIG_REGULATOR_HELPER	BIT(2)
#define OPP_CONFIG_PROP_NAME		BIT(3)
#define OPP_CONFIG_SUPPORTED_HW		BIT(4)
#define OPP_CONFIG_GENPD		BIT(5)
struct opp_config_data {
	struct opp_table *opp_table;
	unsigned int flags;
};
struct dev_pm_opp {
	struct list_head node;
	struct kref kref;
	bool available;
	bool dynamic;
	bool turbo;
	bool suspend;
	bool removed;
	unsigned long *rates;
	unsigned int level;
	struct dev_pm_opp_supply *supplies;
	struct dev_pm_opp_icc_bw *bandwidth;
	unsigned long clock_latency_ns;
	struct dev_pm_opp **required_opps;
	struct opp_table *opp_table;
	struct device_node *np;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
	const char *of_name;
#endif
};
struct opp_device {
	struct list_head node;
	const struct device *dev;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif
};
enum opp_table_access {
	OPP_TABLE_ACCESS_UNKNOWN = 0,
	OPP_TABLE_ACCESS_EXCLUSIVE = 1,
	OPP_TABLE_ACCESS_SHARED = 2,
};
struct opp_table {
	struct list_head node, lazy;
	struct blocking_notifier_head head;
	struct list_head dev_list;
	struct list_head opp_list;
	struct kref kref;
	struct mutex lock;
	struct device_node *np;
	unsigned long clock_latency_ns_max;
	unsigned int voltage_tolerance_v1;
	unsigned int parsed_static_opps;
	enum opp_table_access shared_opp;
	unsigned long rate_clk_single;
	struct dev_pm_opp *current_opp;
	struct dev_pm_opp *suspend_opp;
	struct mutex genpd_virt_dev_lock;
	struct device **genpd_virt_devs;
	struct opp_table **required_opp_tables;
	unsigned int required_opp_count;
	unsigned int *supported_hw;
	unsigned int supported_hw_count;
	const char *prop_name;
	config_clks_t config_clks;
	struct clk **clks;
	struct clk *clk;
	int clk_count;
	config_regulators_t config_regulators;
	struct regulator **regulators;
	int regulator_count;
	struct icc_path **paths;
	unsigned int path_count;
	bool enabled;
	bool is_genpd;
	int (*set_required_opps)(struct device *dev,
		struct opp_table *opp_table, struct dev_pm_opp *opp, bool scaling_down);
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
	char dentry_name[NAME_MAX];
#endif
};
void dev_pm_opp_get(struct dev_pm_opp *opp);
bool _opp_remove_all_static(struct opp_table *opp_table);
void _get_opp_table_kref(struct opp_table *opp_table);
int _get_opp_count(struct opp_table *opp_table);
struct opp_table *_find_opp_table(struct device *dev);
struct opp_device *_add_opp_dev(const struct device *dev, struct opp_table *opp_table);
struct dev_pm_opp *_opp_allocate(struct opp_table *opp_table);
void _opp_free(struct dev_pm_opp *opp);
int _opp_compare_key(struct opp_table *opp_table, struct dev_pm_opp *opp1, struct dev_pm_opp *opp2);
int _opp_add(struct device *dev, struct dev_pm_opp *new_opp, struct opp_table *opp_table);
int _opp_add_v1(struct opp_table *opp_table, struct device *dev, unsigned long freq, long u_volt, bool dynamic);
void _dev_pm_opp_cpumask_remove_table(const struct cpumask *cpumask, int last_cpu);
struct opp_table *_add_opp_table_indexed(struct device *dev, int index, bool getclk);
void _put_opp_list_kref(struct opp_table *opp_table);
void _required_opps_available(struct dev_pm_opp *opp, int count);
void _update_set_required_opps(struct opp_table *opp_table);
static inline bool lazy_linking_pending(struct opp_table *opp_table)
{
	return unlikely(!list_empty(&opp_table->lazy));
}
#ifdef CONFIG_OF
void _of_init_opp_table(struct opp_table *opp_table, struct device *dev, int index);
void _of_clear_opp_table(struct opp_table *opp_table);
struct opp_table *_managed_opp(struct device *dev, int index);
void _of_clear_opp(struct opp_table *opp_table, struct dev_pm_opp *opp);
#else
static inline void _of_init_opp_table(struct opp_table *opp_table, struct device *dev, int index) {}
static inline void _of_clear_opp_table(struct opp_table *opp_table) {}
static inline struct opp_table *_managed_opp(struct device *dev, int index) { return NULL; }
static inline void _of_clear_opp(struct opp_table *opp_table, struct dev_pm_opp *opp) {}
#endif
#ifdef CONFIG_DEBUG_FS
void opp_debug_remove_one(struct dev_pm_opp *opp);
void opp_debug_create_one(struct dev_pm_opp *opp, struct opp_table *opp_table);
void opp_debug_register(struct opp_device *opp_dev, struct opp_table *opp_table);
void opp_debug_unregister(struct opp_device *opp_dev, struct opp_table *opp_table);
#else
static inline void opp_debug_remove_one(struct dev_pm_opp *opp) {}
static inline void opp_debug_create_one(struct dev_pm_opp *opp,
					struct opp_table *opp_table) { }
static inline void opp_debug_register(struct opp_device *opp_dev,
				      struct opp_table *opp_table) { }
static inline void opp_debug_unregister(struct opp_device *opp_dev,
					struct opp_table *opp_table)
{ }
#endif		 
#endif		 
