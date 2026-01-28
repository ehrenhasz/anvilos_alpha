#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_HWMOD_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_HWMOD_H
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
struct omap_device;
extern struct sysc_regbits omap_hwmod_sysc_type1;
extern struct sysc_regbits omap_hwmod_sysc_type2;
extern struct sysc_regbits omap_hwmod_sysc_type3;
extern struct sysc_regbits omap34xx_sr_sysc_fields;
extern struct sysc_regbits omap36xx_sr_sysc_fields;
extern struct sysc_regbits omap3_sham_sysc_fields;
extern struct sysc_regbits omap3xxx_aes_sysc_fields;
extern struct sysc_regbits omap_hwmod_sysc_type_mcasp;
extern struct sysc_regbits omap_hwmod_sysc_type_usb_host_fs;
#define SYSC_TYPE1_MIDLEMODE_SHIFT	12
#define SYSC_TYPE1_MIDLEMODE_MASK	(0x3 << SYSC_TYPE1_MIDLEMODE_SHIFT)
#define SYSC_TYPE1_CLOCKACTIVITY_SHIFT	8
#define SYSC_TYPE1_CLOCKACTIVITY_MASK	(0x3 << SYSC_TYPE1_CLOCKACTIVITY_SHIFT)
#define SYSC_TYPE1_SIDLEMODE_SHIFT	3
#define SYSC_TYPE1_SIDLEMODE_MASK	(0x3 << SYSC_TYPE1_SIDLEMODE_SHIFT)
#define SYSC_TYPE1_ENAWAKEUP_SHIFT	2
#define SYSC_TYPE1_ENAWAKEUP_MASK	(1 << SYSC_TYPE1_ENAWAKEUP_SHIFT)
#define SYSC_TYPE1_SOFTRESET_SHIFT	1
#define SYSC_TYPE1_SOFTRESET_MASK	(1 << SYSC_TYPE1_SOFTRESET_SHIFT)
#define SYSC_TYPE1_AUTOIDLE_SHIFT	0
#define SYSC_TYPE1_AUTOIDLE_MASK	(1 << SYSC_TYPE1_AUTOIDLE_SHIFT)
#define SYSC_TYPE2_SOFTRESET_SHIFT	0
#define SYSC_TYPE2_SOFTRESET_MASK	(1 << SYSC_TYPE2_SOFTRESET_SHIFT)
#define SYSC_TYPE2_SIDLEMODE_SHIFT	2
#define SYSC_TYPE2_SIDLEMODE_MASK	(0x3 << SYSC_TYPE2_SIDLEMODE_SHIFT)
#define SYSC_TYPE2_MIDLEMODE_SHIFT	4
#define SYSC_TYPE2_MIDLEMODE_MASK	(0x3 << SYSC_TYPE2_MIDLEMODE_SHIFT)
#define SYSC_TYPE2_DMADISABLE_SHIFT	16
#define SYSC_TYPE2_DMADISABLE_MASK	(0x1 << SYSC_TYPE2_DMADISABLE_SHIFT)
#define SYSC_TYPE3_SIDLEMODE_SHIFT	0
#define SYSC_TYPE3_SIDLEMODE_MASK	(0x3 << SYSC_TYPE3_SIDLEMODE_SHIFT)
#define SYSC_TYPE3_MIDLEMODE_SHIFT	2
#define SYSC_TYPE3_MIDLEMODE_MASK	(0x3 << SYSC_TYPE3_MIDLEMODE_SHIFT)
#define SYSS_RESETDONE_SHIFT		0
#define SYSS_RESETDONE_MASK		(1 << SYSS_RESETDONE_SHIFT)
#define HWMOD_IDLEMODE_FORCE		(1 << 0)
#define HWMOD_IDLEMODE_NO		(1 << 1)
#define HWMOD_IDLEMODE_SMART		(1 << 2)
#define HWMOD_IDLEMODE_SMART_WKUP	(1 << 3)
#define MODULEMODE_HWCTRL		1
#define MODULEMODE_SWCTRL		2
#define DEBUG_OMAP2UART1_FLAGS	0
#define DEBUG_OMAP2UART2_FLAGS	0
#define DEBUG_OMAP2UART3_FLAGS	0
#define DEBUG_OMAP3UART3_FLAGS	0
#define DEBUG_OMAP3UART4_FLAGS	0
#define DEBUG_OMAP4UART3_FLAGS	0
#define DEBUG_OMAP4UART4_FLAGS	0
#define DEBUG_TI81XXUART1_FLAGS	0
#define DEBUG_TI81XXUART2_FLAGS	0
#define DEBUG_TI81XXUART3_FLAGS	0
#define DEBUG_AM33XXUART1_FLAGS	0
#define DEBUG_OMAPUART_FLAGS	(HWMOD_INIT_NO_IDLE | HWMOD_INIT_NO_RESET)
#ifdef CONFIG_OMAP_GPMC_DEBUG
#define DEBUG_OMAP_GPMC_HWMOD_FLAGS	HWMOD_INIT_NO_RESET
#else
#define DEBUG_OMAP_GPMC_HWMOD_FLAGS	0
#endif
#if defined(CONFIG_DEBUG_OMAP2UART1)
#undef DEBUG_OMAP2UART1_FLAGS
#define DEBUG_OMAP2UART1_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_OMAP2UART2)
#undef DEBUG_OMAP2UART2_FLAGS
#define DEBUG_OMAP2UART2_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_OMAP2UART3)
#undef DEBUG_OMAP2UART3_FLAGS
#define DEBUG_OMAP2UART3_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_OMAP3UART3)
#undef DEBUG_OMAP3UART3_FLAGS
#define DEBUG_OMAP3UART3_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_OMAP3UART4)
#undef DEBUG_OMAP3UART4_FLAGS
#define DEBUG_OMAP3UART4_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_OMAP4UART3)
#undef DEBUG_OMAP4UART3_FLAGS
#define DEBUG_OMAP4UART3_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_OMAP4UART4)
#undef DEBUG_OMAP4UART4_FLAGS
#define DEBUG_OMAP4UART4_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_TI81XXUART1)
#undef DEBUG_TI81XXUART1_FLAGS
#define DEBUG_TI81XXUART1_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_TI81XXUART2)
#undef DEBUG_TI81XXUART2_FLAGS
#define DEBUG_TI81XXUART2_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_TI81XXUART3)
#undef DEBUG_TI81XXUART3_FLAGS
#define DEBUG_TI81XXUART3_FLAGS DEBUG_OMAPUART_FLAGS
#elif defined(CONFIG_DEBUG_AM33XXUART1)
#undef DEBUG_AM33XXUART1_FLAGS
#define DEBUG_AM33XXUART1_FLAGS DEBUG_OMAPUART_FLAGS
#endif
struct omap_hwmod_rst_info {
	const char	*name;
	u8		rst_shift;
	u8		st_shift;
};
struct omap_hwmod_opt_clk {
	const char	*role;
	const char	*clk;
	struct clk	*_clk;
};
#define OMAP_FIREWALL_L3		(1 << 0)
#define OMAP_FIREWALL_L4		(1 << 1)
struct omap_hwmod_omap2_firewall {
	u8 l3_perm_bit;
	u8 l4_fw_region;
	u8 l4_prot_group;
	u8 flags;
};
#define OCP_USER_MPU			(1 << 0)
#define OCP_USER_SDMA			(1 << 1)
#define OCP_USER_DSP			(1 << 2)
#define OCP_USER_IVA			(1 << 3)
#define OCPIF_SWSUP_IDLE		(1 << 0)
#define OCPIF_CAN_BURST			(1 << 1)
#define _OCPIF_INT_FLAGS_REGISTERED	(1 << 0)
struct omap_hwmod_ocp_if {
	struct omap_hwmod		*master;
	struct omap_hwmod		*slave;
	struct omap_hwmod_addr_space	*addr;
	const char			*clk;
	struct clk			*_clk;
	struct list_head		node;
	union {
		struct omap_hwmod_omap2_firewall omap2;
	}				fw;
	u8				width;
	u8				user;
	u8				flags;
	u8				_int_flags;
};
#define MASTER_STANDBY_SHIFT	4
#define SLAVE_IDLE_SHIFT	0
#define SIDLE_FORCE		(HWMOD_IDLEMODE_FORCE << SLAVE_IDLE_SHIFT)
#define SIDLE_NO		(HWMOD_IDLEMODE_NO << SLAVE_IDLE_SHIFT)
#define SIDLE_SMART		(HWMOD_IDLEMODE_SMART << SLAVE_IDLE_SHIFT)
#define SIDLE_SMART_WKUP	(HWMOD_IDLEMODE_SMART_WKUP << SLAVE_IDLE_SHIFT)
#define MSTANDBY_FORCE		(HWMOD_IDLEMODE_FORCE << MASTER_STANDBY_SHIFT)
#define MSTANDBY_NO		(HWMOD_IDLEMODE_NO << MASTER_STANDBY_SHIFT)
#define MSTANDBY_SMART		(HWMOD_IDLEMODE_SMART << MASTER_STANDBY_SHIFT)
#define MSTANDBY_SMART_WKUP	(HWMOD_IDLEMODE_SMART_WKUP << MASTER_STANDBY_SHIFT)
#define SYSC_HAS_AUTOIDLE	(1 << 0)
#define SYSC_HAS_SOFTRESET	(1 << 1)
#define SYSC_HAS_ENAWAKEUP	(1 << 2)
#define SYSC_HAS_EMUFREE	(1 << 3)
#define SYSC_HAS_CLOCKACTIVITY	(1 << 4)
#define SYSC_HAS_SIDLEMODE	(1 << 5)
#define SYSC_HAS_MIDLEMODE	(1 << 6)
#define SYSS_HAS_RESET_STATUS	(1 << 7)
#define SYSC_NO_CACHE		(1 << 8)   
#define SYSC_HAS_RESET_STATUS	(1 << 9)
#define SYSC_HAS_DMADISABLE	(1 << 10)
#define CLOCKACT_TEST_BOTH	0x0
#define CLOCKACT_TEST_MAIN	0x1
#define CLOCKACT_TEST_ICLK	0x2
#define CLOCKACT_TEST_NONE	0x3
struct omap_hwmod_class_sysconfig {
	s32 rev_offs;
	s32 sysc_offs;
	s32 syss_offs;
	u16 sysc_flags;
	struct sysc_regbits *sysc_fields;
	u8 srst_udelay;
	u8 idlemodes;
};
struct omap_hwmod_omap2_prcm {
	s16 module_offs;
	u8 idlest_reg_id;
	u8 idlest_idle_bit;
};
#define HWMOD_OMAP4_NO_CONTEXT_LOSS_BIT		(1 << 0)
#define HWMOD_OMAP4_ZERO_CLKCTRL_OFFSET		(1 << 1)
#define HWMOD_OMAP4_CLKFWK_CLKCTR_CLOCK		(1 << 2)
struct omap_hwmod_omap4_prcm {
	u16		clkctrl_offs;
	u16		rstctrl_offs;
	u16		rstst_offs;
	u16		context_offs;
	u32		lostcontext_mask;
	u8		submodule_wkdep_bit;
	u8		modulemode;
	u8		flags;
	int		context_lost_counter;
};
#define HWMOD_SWSUP_SIDLE			(1 << 0)
#define HWMOD_SWSUP_MSTANDBY			(1 << 1)
#define HWMOD_INIT_NO_RESET			(1 << 2)
#define HWMOD_INIT_NO_IDLE			(1 << 3)
#define HWMOD_NO_OCP_AUTOIDLE			(1 << 4)
#define HWMOD_SET_DEFAULT_CLOCKACT		(1 << 5)
#define HWMOD_NO_IDLEST				(1 << 6)
#define HWMOD_CONTROL_OPT_CLKS_IN_RESET		(1 << 7)
#define HWMOD_16BIT_REG				(1 << 8)
#define HWMOD_EXT_OPT_MAIN_CLK			(1 << 9)
#define HWMOD_BLOCK_WFI				(1 << 10)
#define HWMOD_FORCE_MSTANDBY			(1 << 11)
#define HWMOD_SWSUP_SIDLE_ACT			(1 << 12)
#define HWMOD_RECONFIG_IO_CHAIN			(1 << 13)
#define HWMOD_OPT_CLKS_NEEDED			(1 << 14)
#define HWMOD_NO_IDLE				(1 << 15)
#define HWMOD_CLKDM_NOAUTO			(1 << 16)
#define _HWMOD_NO_MPU_PORT			(1 << 0)
#define _HWMOD_SYSCONFIG_LOADED			(1 << 1)
#define _HWMOD_SKIP_ENABLE			(1 << 2)
#define _HWMOD_STATE_UNKNOWN			0
#define _HWMOD_STATE_REGISTERED			1
#define _HWMOD_STATE_CLKS_INITED		2
#define _HWMOD_STATE_INITIALIZED		3
#define _HWMOD_STATE_ENABLED			4
#define _HWMOD_STATE_IDLE			5
#define _HWMOD_STATE_DISABLED			6
#ifdef CONFIG_PM
#define _HWMOD_STATE_DEFAULT			_HWMOD_STATE_IDLE
#else
#define _HWMOD_STATE_DEFAULT			_HWMOD_STATE_ENABLED
#endif
struct omap_hwmod_class {
	const char				*name;
	struct omap_hwmod_class_sysconfig	*sysc;
	int					(*pre_shutdown)(struct omap_hwmod *oh);
	int					(*reset)(struct omap_hwmod *oh);
	void					(*lock)(struct omap_hwmod *oh);
	void					(*unlock)(struct omap_hwmod *oh);
};
struct omap_hwmod {
	const char			*name;
	struct omap_hwmod_class		*class;
	struct omap_device		*od;
	struct omap_hwmod_rst_info	*rst_lines;
	union {
		struct omap_hwmod_omap2_prcm omap2;
		struct omap_hwmod_omap4_prcm omap4;
	}				prcm;
	const char			*main_clk;
	struct clk			*_clk;
	struct omap_hwmod_opt_clk	*opt_clks;
	const char			*clkdm_name;
	struct clockdomain		*clkdm;
	struct list_head		slave_ports;  
	void				*dev_attr;
	u32				_sysc_cache;
	void __iomem			*_mpu_rt_va;
	spinlock_t			_lock;
	struct lock_class_key		hwmod_key;  
	struct list_head		node;
	struct omap_hwmod_ocp_if	*_mpu_port;
	u32				flags;
	u8				mpu_rt_idx;
	u8				response_lat;
	u8				rst_lines_cnt;
	u8				opt_clks_cnt;
	u8				slaves_cnt;
	u8				hwmods_cnt;
	u8				_int_flags;
	u8				_state;
	u8				_postsetup_state;
	struct omap_hwmod		*parent_hwmod;
};
#ifdef CONFIG_OMAP_HWMOD
struct device_node;
struct omap_hwmod *omap_hwmod_lookup(const char *name);
int omap_hwmod_for_each(int (*fn)(struct omap_hwmod *oh, void *data),
			void *data);
int omap_hwmod_parse_module_range(struct omap_hwmod *oh,
				  struct device_node *np,
				  struct resource *res);
struct ti_sysc_module_data;
struct ti_sysc_cookie;
int omap_hwmod_init_module(struct device *dev,
			   const struct ti_sysc_module_data *data,
			   struct ti_sysc_cookie *cookie);
int omap_hwmod_enable(struct omap_hwmod *oh);
int omap_hwmod_idle(struct omap_hwmod *oh);
int omap_hwmod_shutdown(struct omap_hwmod *oh);
int omap_hwmod_assert_hardreset(struct omap_hwmod *oh, const char *name);
int omap_hwmod_deassert_hardreset(struct omap_hwmod *oh, const char *name);
void omap_hwmod_write(u32 v, struct omap_hwmod *oh, u16 reg_offs);
u32 omap_hwmod_read(struct omap_hwmod *oh, u16 reg_offs);
int omap_hwmod_softreset(struct omap_hwmod *oh);
void __iomem *omap_hwmod_get_mpu_rt_va(struct omap_hwmod *oh);
int omap_hwmod_for_each_by_class(const char *classname,
				 int (*fn)(struct omap_hwmod *oh,
					   void *user),
				 void *user);
int omap_hwmod_set_postsetup_state(struct omap_hwmod *oh, u8 state);
extern void __init omap_hwmod_init(void);
#else	 
static inline int
omap_hwmod_for_each_by_class(const char *classname,
			     int (*fn)(struct omap_hwmod *oh, void *user),
			     void *user)
{
	return 0;
}
#endif	 
extern int omap2420_hwmod_init(void);
extern int omap2430_hwmod_init(void);
extern int omap3xxx_hwmod_init(void);
extern int dm814x_hwmod_init(void);
extern int dm816x_hwmod_init(void);
extern int __init omap_hwmod_register_links(struct omap_hwmod_ocp_if **ois);
#endif
