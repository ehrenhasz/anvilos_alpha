
 

#include <linux/arm-smccc.h>
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/hashtable.h>

#include <linux/firmware/xlnx-zynqmp.h>
#include <linux/firmware/xlnx-event-manager.h>
#include "zynqmp-debug.h"

 
#define PM_API_FEATURE_CHECK_MAX_ORDER  7

 
#define CRL_APB_BASE			0xFF5E0000U
 
#define CRL_APB_BOOT_PIN_CTRL		(CRL_APB_BASE + (0x250U))
 
#define CRL_APB_BOOTPIN_CTRL_MASK	0xF0FU

 
#define FEATURE_PAYLOAD_SIZE		2

 
#define FIRMWARE_VERSION_MASK		GENMASK(15, 0)

static bool feature_check_enabled;
static DEFINE_HASHTABLE(pm_api_features_map, PM_API_FEATURE_CHECK_MAX_ORDER);
static u32 ioctl_features[FEATURE_PAYLOAD_SIZE];
static u32 query_features[FEATURE_PAYLOAD_SIZE];

static struct platform_device *em_dev;

 
struct zynqmp_devinfo {
	struct device *dev;
	u32 feature_conf_id;
};

 
struct pm_api_feature_data {
	u32 pm_api_id;
	int feature_status;
	struct hlist_node hentry;
};

static const struct mfd_cell firmware_devs[] = {
	{
		.name = "zynqmp_power_controller",
	},
};

 
static int zynqmp_pm_ret_code(u32 ret_status)
{
	switch (ret_status) {
	case XST_PM_SUCCESS:
	case XST_PM_DOUBLE_REQ:
		return 0;
	case XST_PM_NO_FEATURE:
		return -ENOTSUPP;
	case XST_PM_NO_ACCESS:
		return -EACCES;
	case XST_PM_ABORT_SUSPEND:
		return -ECANCELED;
	case XST_PM_MULT_USER:
		return -EUSERS;
	case XST_PM_INTERNAL:
	case XST_PM_CONFLICT:
	case XST_PM_INVALID_NODE:
	default:
		return -EINVAL;
	}
}

static noinline int do_fw_call_fail(u64 arg0, u64 arg1, u64 arg2,
				    u32 *ret_payload)
{
	return -ENODEV;
}

 
static int (*do_fw_call)(u64, u64, u64, u32 *ret_payload) = do_fw_call_fail;

 
static noinline int do_fw_call_smc(u64 arg0, u64 arg1, u64 arg2,
				   u32 *ret_payload)
{
	struct arm_smccc_res res;

	arm_smccc_smc(arg0, arg1, arg2, 0, 0, 0, 0, 0, &res);

	if (ret_payload) {
		ret_payload[0] = lower_32_bits(res.a0);
		ret_payload[1] = upper_32_bits(res.a0);
		ret_payload[2] = lower_32_bits(res.a1);
		ret_payload[3] = upper_32_bits(res.a1);
	}

	return zynqmp_pm_ret_code((enum pm_ret_status)res.a0);
}

 
static noinline int do_fw_call_hvc(u64 arg0, u64 arg1, u64 arg2,
				   u32 *ret_payload)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(arg0, arg1, arg2, 0, 0, 0, 0, 0, &res);

	if (ret_payload) {
		ret_payload[0] = lower_32_bits(res.a0);
		ret_payload[1] = upper_32_bits(res.a0);
		ret_payload[2] = lower_32_bits(res.a1);
		ret_payload[3] = upper_32_bits(res.a1);
	}

	return zynqmp_pm_ret_code((enum pm_ret_status)res.a0);
}

static int __do_feature_check_call(const u32 api_id, u32 *ret_payload)
{
	int ret;
	u64 smc_arg[2];

	smc_arg[0] = PM_SIP_SVC | PM_FEATURE_CHECK;
	smc_arg[1] = api_id;

	ret = do_fw_call(smc_arg[0], smc_arg[1], 0, ret_payload);
	if (ret)
		ret = -EOPNOTSUPP;
	else
		ret = ret_payload[1];

	return ret;
}

static int do_feature_check_call(const u32 api_id)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];
	struct pm_api_feature_data *feature_data;

	 
	hash_for_each_possible(pm_api_features_map, feature_data, hentry,
			       api_id) {
		if (feature_data->pm_api_id == api_id)
			return feature_data->feature_status;
	}

	 
	feature_data = kmalloc(sizeof(*feature_data), GFP_ATOMIC);
	if (!feature_data)
		return -ENOMEM;

	feature_data->pm_api_id = api_id;
	ret = __do_feature_check_call(api_id, ret_payload);

	feature_data->feature_status = ret;
	hash_add(pm_api_features_map, &feature_data->hentry, api_id);

	if (api_id == PM_IOCTL)
		 
		memcpy(ioctl_features, &ret_payload[2], FEATURE_PAYLOAD_SIZE * 4);
	else if (api_id == PM_QUERY_DATA)
		 
		memcpy(query_features, &ret_payload[2], FEATURE_PAYLOAD_SIZE * 4);

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_feature);

 
int zynqmp_pm_feature(const u32 api_id)
{
	int ret;

	if (!feature_check_enabled)
		return 0;

	ret = do_feature_check_call(api_id);

	return ret;
}

 
int zynqmp_pm_is_function_supported(const u32 api_id, const u32 id)
{
	int ret;
	u32 *bit_mask;

	 
	if (id >= 64 || (api_id != PM_IOCTL && api_id != PM_QUERY_DATA))
		return -EINVAL;

	 
	ret = do_feature_check_call(PM_FEATURE_CHECK);
	if (ret < 0)
		return ret;

	 
	if ((ret & FIRMWARE_VERSION_MASK) == PM_API_VERSION_2) {
		 
		ret = do_feature_check_call(api_id);
		if (ret < 0)
			return ret;

		bit_mask = (api_id == PM_IOCTL) ? ioctl_features : query_features;

		if ((bit_mask[(id / 32)] & BIT((id % 32))) == 0U)
			return -EOPNOTSUPP;
	} else {
		return -ENODATA;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_is_function_supported);

 
int zynqmp_pm_invoke_fn(u32 pm_api_id, u32 arg0, u32 arg1,
			u32 arg2, u32 arg3, u32 *ret_payload)
{
	 
	u64 smc_arg[4];
	int ret;

	 
	ret = zynqmp_pm_feature(pm_api_id);
	if (ret < 0)
		return ret;

	smc_arg[0] = PM_SIP_SVC | pm_api_id;
	smc_arg[1] = ((u64)arg1 << 32) | arg0;
	smc_arg[2] = ((u64)arg3 << 32) | arg2;

	return do_fw_call(smc_arg[0], smc_arg[1], smc_arg[2], ret_payload);
}

static u32 pm_api_version;
static u32 pm_tz_version;
static u32 pm_family_code;
static u32 pm_sub_family_code;

int zynqmp_pm_register_sgi(u32 sgi_num, u32 reset)
{
	int ret;

	ret = zynqmp_pm_invoke_fn(TF_A_PM_REGISTER_SGI, sgi_num, reset, 0, 0,
				  NULL);
	if (!ret)
		return ret;

	 
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_REGISTER_SGI, sgi_num,
				   reset, NULL);
}

 
int zynqmp_pm_get_api_version(u32 *version)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!version)
		return -EINVAL;

	 
	if (pm_api_version > 0) {
		*version = pm_api_version;
		return 0;
	}
	ret = zynqmp_pm_invoke_fn(PM_GET_API_VERSION, 0, 0, 0, 0, ret_payload);
	*version = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_get_api_version);

 
int zynqmp_pm_get_chipid(u32 *idcode, u32 *version)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!idcode || !version)
		return -EINVAL;

	ret = zynqmp_pm_invoke_fn(PM_GET_CHIPID, 0, 0, 0, 0, ret_payload);
	*idcode = ret_payload[1];
	*version = ret_payload[2];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_get_chipid);

 
static int zynqmp_pm_get_family_info(u32 *family, u32 *subfamily)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	u32 idcode;
	int ret;

	 
	if (pm_family_code && pm_sub_family_code) {
		*family = pm_family_code;
		*subfamily = pm_sub_family_code;
		return 0;
	}

	ret = zynqmp_pm_invoke_fn(PM_GET_CHIPID, 0, 0, 0, 0, ret_payload);
	if (ret < 0)
		return ret;

	idcode = ret_payload[1];
	pm_family_code = FIELD_GET(FAMILY_CODE_MASK, idcode);
	pm_sub_family_code = FIELD_GET(SUB_FAMILY_CODE_MASK, idcode);
	*family = pm_family_code;
	*subfamily = pm_sub_family_code;

	return 0;
}

 
static int zynqmp_pm_get_trustzone_version(u32 *version)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!version)
		return -EINVAL;

	 
	if (pm_tz_version > 0) {
		*version = pm_tz_version;
		return 0;
	}
	ret = zynqmp_pm_invoke_fn(PM_GET_TRUSTZONE_VERSION, 0, 0,
				  0, 0, ret_payload);
	*version = ret_payload[1];

	return ret;
}

 
static int get_set_conduit_method(struct device_node *np)
{
	const char *method;

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("%s missing \"method\" property\n", __func__);
		return -ENXIO;
	}

	if (!strcmp("hvc", method)) {
		do_fw_call = do_fw_call_hvc;
	} else if (!strcmp("smc", method)) {
		do_fw_call = do_fw_call_smc;
	} else {
		pr_warn("%s Invalid \"method\" property: %s\n",
			__func__, method);
		return -EINVAL;
	}

	return 0;
}

 
int zynqmp_pm_query_data(struct zynqmp_pm_query_data qdata, u32 *out)
{
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_QUERY_DATA, qdata.qid, qdata.arg1,
				  qdata.arg2, qdata.arg3, out);

	 
	return qdata.qid == PM_QID_CLOCK_GET_NAME ? 0 : ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_query_data);

 
int zynqmp_pm_clock_enable(u32 clock_id)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_ENABLE, clock_id, 0, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_enable);

 
int zynqmp_pm_clock_disable(u32 clock_id)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_DISABLE, clock_id, 0, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_disable);

 
int zynqmp_pm_clock_getstate(u32 clock_id, u32 *state)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETSTATE, clock_id, 0,
				  0, 0, ret_payload);
	*state = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_getstate);

 
int zynqmp_pm_clock_setdivider(u32 clock_id, u32 divider)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_SETDIVIDER, clock_id, divider,
				   0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_setdivider);

 
int zynqmp_pm_clock_getdivider(u32 clock_id, u32 *divider)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETDIVIDER, clock_id, 0,
				  0, 0, ret_payload);
	*divider = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_getdivider);

 
int zynqmp_pm_clock_setrate(u32 clock_id, u64 rate)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_SETRATE, clock_id,
				   lower_32_bits(rate),
				   upper_32_bits(rate),
				   0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_setrate);

 
int zynqmp_pm_clock_getrate(u32 clock_id, u64 *rate)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETRATE, clock_id, 0,
				  0, 0, ret_payload);
	*rate = ((u64)ret_payload[2] << 32) | ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_getrate);

 
int zynqmp_pm_clock_setparent(u32 clock_id, u32 parent_id)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_SETPARENT, clock_id,
				   parent_id, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_setparent);

 
int zynqmp_pm_clock_getparent(u32 clock_id, u32 *parent_id)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETPARENT, clock_id, 0,
				  0, 0, ret_payload);
	*parent_id = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_clock_getparent);

 
int zynqmp_pm_set_pll_frac_mode(u32 clk_id, u32 mode)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_SET_PLL_FRAC_MODE,
				   clk_id, mode, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_pll_frac_mode);

 
int zynqmp_pm_get_pll_frac_mode(u32 clk_id, u32 *mode)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_GET_PLL_FRAC_MODE,
				   clk_id, 0, mode);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_get_pll_frac_mode);

 
int zynqmp_pm_set_pll_frac_data(u32 clk_id, u32 data)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_SET_PLL_FRAC_DATA,
				   clk_id, data, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_pll_frac_data);

 
int zynqmp_pm_get_pll_frac_data(u32 clk_id, u32 *data)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_GET_PLL_FRAC_DATA,
				   clk_id, 0, data);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_get_pll_frac_data);

 
int zynqmp_pm_set_sd_tapdelay(u32 node_id, u32 type, u32 value)
{
	u32 reg = (type == PM_TAPDELAY_INPUT) ? SD_ITAPDLY : SD_OTAPDLYSEL;
	u32 mask = (node_id == NODE_SD_0) ? GENMASK(15, 0) : GENMASK(31, 16);

	if (value) {
		return zynqmp_pm_invoke_fn(PM_IOCTL, node_id,
					   IOCTL_SET_SD_TAPDELAY,
					   type, value, NULL);
	}

	 
	return zynqmp_pm_invoke_fn(PM_MMIO_WRITE, reg, mask, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_sd_tapdelay);

 
int zynqmp_pm_sd_dll_reset(u32 node_id, u32 type)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, node_id, IOCTL_SD_DLL_RESET,
				   type, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_sd_dll_reset);

 
int zynqmp_pm_ospi_mux_select(u32 dev_id, u32 select)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, dev_id, IOCTL_OSPI_MUX_SELECT,
				   select, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_ospi_mux_select);

 
int zynqmp_pm_write_ggs(u32 index, u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_WRITE_GGS,
				   index, value, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_write_ggs);

 
int zynqmp_pm_read_ggs(u32 index, u32 *value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_READ_GGS,
				   index, 0, value);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_read_ggs);

 
int zynqmp_pm_write_pggs(u32 index, u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_WRITE_PGGS, index, value,
				   NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_write_pggs);

 
int zynqmp_pm_read_pggs(u32 index, u32 *value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_READ_PGGS, index, 0,
				   value);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_read_pggs);

int zynqmp_pm_set_tapdelay_bypass(u32 index, u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_SET_TAPDELAY_BYPASS,
				   index, value, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_tapdelay_bypass);

 
int zynqmp_pm_set_boot_health_status(u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_SET_BOOT_HEALTH_STATUS,
				   value, 0, NULL);
}

 
int zynqmp_pm_reset_assert(const enum zynqmp_pm_reset reset,
			   const enum zynqmp_pm_reset_action assert_flag)
{
	return zynqmp_pm_invoke_fn(PM_RESET_ASSERT, reset, assert_flag,
				   0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_reset_assert);

 
int zynqmp_pm_reset_get_status(const enum zynqmp_pm_reset reset, u32 *status)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!status)
		return -EINVAL;

	ret = zynqmp_pm_invoke_fn(PM_RESET_GET_STATUS, reset, 0,
				  0, 0, ret_payload);
	*status = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_reset_get_status);

 
int zynqmp_pm_fpga_load(const u64 address, const u32 size, const u32 flags)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_FPGA_LOAD, lower_32_bits(address),
				  upper_32_bits(address), size, flags,
				  ret_payload);
	if (ret_payload[0])
		return -ret_payload[0];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_fpga_load);

 
int zynqmp_pm_fpga_get_status(u32 *value)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!value)
		return -EINVAL;

	ret = zynqmp_pm_invoke_fn(PM_FPGA_GET_STATUS, 0, 0, 0, 0, ret_payload);
	*value = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_fpga_get_status);

 
int zynqmp_pm_fpga_get_config_status(u32 *value)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	u32 buf, lower_addr, upper_addr;
	int ret;

	if (!value)
		return -EINVAL;

	lower_addr = lower_32_bits((u64)&buf);
	upper_addr = upper_32_bits((u64)&buf);

	ret = zynqmp_pm_invoke_fn(PM_FPGA_READ,
				  XILINX_ZYNQMP_PM_FPGA_CONFIG_STAT_OFFSET,
				  lower_addr, upper_addr,
				  XILINX_ZYNQMP_PM_FPGA_READ_CONFIG_REG,
				  ret_payload);

	*value = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_fpga_get_config_status);

 
int zynqmp_pm_pinctrl_request(const u32 pin)
{
	return zynqmp_pm_invoke_fn(PM_PINCTRL_REQUEST, pin, 0, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_pinctrl_request);

 
int zynqmp_pm_pinctrl_release(const u32 pin)
{
	return zynqmp_pm_invoke_fn(PM_PINCTRL_RELEASE, pin, 0, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_pinctrl_release);

 
int zynqmp_pm_pinctrl_get_function(const u32 pin, u32 *id)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!id)
		return -EINVAL;

	ret = zynqmp_pm_invoke_fn(PM_PINCTRL_GET_FUNCTION, pin, 0,
				  0, 0, ret_payload);
	*id = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_pinctrl_get_function);

 
int zynqmp_pm_pinctrl_set_function(const u32 pin, const u32 id)
{
	return zynqmp_pm_invoke_fn(PM_PINCTRL_SET_FUNCTION, pin, id,
				   0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_pinctrl_set_function);

 
int zynqmp_pm_pinctrl_get_config(const u32 pin, const u32 param,
				 u32 *value)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!value)
		return -EINVAL;

	ret = zynqmp_pm_invoke_fn(PM_PINCTRL_CONFIG_PARAM_GET, pin, param,
				  0, 0, ret_payload);
	*value = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_pinctrl_get_config);

 
int zynqmp_pm_pinctrl_set_config(const u32 pin, const u32 param,
				 u32 value)
{
	int ret;

	if (pm_family_code == ZYNQMP_FAMILY_CODE &&
	    param == PM_PINCTRL_CONFIG_TRI_STATE) {
		ret = zynqmp_pm_feature(PM_PINCTRL_CONFIG_PARAM_SET);
		if (ret < PM_PINCTRL_PARAM_SET_VERSION)
			return -EOPNOTSUPP;
	}

	return zynqmp_pm_invoke_fn(PM_PINCTRL_CONFIG_PARAM_SET, pin,
				   param, value, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_pinctrl_set_config);

 
unsigned int zynqmp_pm_bootmode_read(u32 *ps_mode)
{
	unsigned int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	ret = zynqmp_pm_invoke_fn(PM_MMIO_READ, CRL_APB_BOOT_PIN_CTRL, 0,
				  0, 0, ret_payload);

	*ps_mode = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_bootmode_read);

 
int zynqmp_pm_bootmode_write(u32 ps_mode)
{
	return zynqmp_pm_invoke_fn(PM_MMIO_WRITE, CRL_APB_BOOT_PIN_CTRL,
				   CRL_APB_BOOTPIN_CTRL_MASK, ps_mode, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_bootmode_write);

 
int zynqmp_pm_init_finalize(void)
{
	return zynqmp_pm_invoke_fn(PM_PM_INIT_FINALIZE, 0, 0, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_init_finalize);

 
int zynqmp_pm_set_suspend_mode(u32 mode)
{
	return zynqmp_pm_invoke_fn(PM_SET_SUSPEND_MODE, mode, 0, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_suspend_mode);

 
int zynqmp_pm_request_node(const u32 node, const u32 capabilities,
			   const u32 qos, const enum zynqmp_pm_request_ack ack)
{
	return zynqmp_pm_invoke_fn(PM_REQUEST_NODE, node, capabilities,
				   qos, ack, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_request_node);

 
int zynqmp_pm_release_node(const u32 node)
{
	return zynqmp_pm_invoke_fn(PM_RELEASE_NODE, node, 0, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_release_node);

 
int zynqmp_pm_get_rpu_mode(u32 node_id, enum rpu_oper_mode *rpu_mode)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_IOCTL, node_id,
				  IOCTL_GET_RPU_OPER_MODE, 0, 0, ret_payload);

	 
	if (ret == XST_PM_SUCCESS)
		*rpu_mode = ret_payload[0];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_get_rpu_mode);

 
int zynqmp_pm_set_rpu_mode(u32 node_id, enum rpu_oper_mode rpu_mode)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, node_id,
				   IOCTL_SET_RPU_OPER_MODE, (u32)rpu_mode,
				   0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_rpu_mode);

 
int zynqmp_pm_set_tcm_config(u32 node_id, enum rpu_tcm_comb tcm_mode)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, node_id,
				   IOCTL_TCM_COMB_CONFIG, (u32)tcm_mode, 0,
				   NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_tcm_config);

 
int zynqmp_pm_force_pwrdwn(const u32 node,
			   const enum zynqmp_pm_request_ack ack)
{
	return zynqmp_pm_invoke_fn(PM_FORCE_POWERDOWN, node, ack, 0, 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_force_pwrdwn);

 
int zynqmp_pm_request_wake(const u32 node,
			   const bool set_addr,
			   const u64 address,
			   const enum zynqmp_pm_request_ack ack)
{
	 
	return zynqmp_pm_invoke_fn(PM_REQUEST_WAKEUP, node, address | set_addr,
				   address >> 32, ack, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_request_wake);

 
int zynqmp_pm_set_requirement(const u32 node, const u32 capabilities,
			      const u32 qos,
			      const enum zynqmp_pm_request_ack ack)
{
	return zynqmp_pm_invoke_fn(PM_SET_REQUIREMENT, node, capabilities,
				   qos, ack, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_requirement);

 
int zynqmp_pm_load_pdi(const u32 src, const u64 address)
{
	return zynqmp_pm_invoke_fn(PM_LOAD_PDI, src,
				   lower_32_bits(address),
				   upper_32_bits(address), 0, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_load_pdi);

 
int zynqmp_pm_aes_engine(const u64 address, u32 *out)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!out)
		return -EINVAL;

	ret = zynqmp_pm_invoke_fn(PM_SECURE_AES, upper_32_bits(address),
				  lower_32_bits(address),
				  0, 0, ret_payload);
	*out = ret_payload[1];

	return ret;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_aes_engine);

 
int zynqmp_pm_sha_hash(const u64 address, const u32 size, const u32 flags)
{
	u32 lower_addr = lower_32_bits(address);
	u32 upper_addr = upper_32_bits(address);

	return zynqmp_pm_invoke_fn(PM_SECURE_SHA, upper_addr, lower_addr,
				   size, flags, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_sha_hash);

 

int zynqmp_pm_register_notifier(const u32 node, const u32 event,
				const u32 wake, const u32 enable)
{
	return zynqmp_pm_invoke_fn(PM_REGISTER_NOTIFIER, node, event,
				   wake, enable, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_register_notifier);

 
int zynqmp_pm_system_shutdown(const u32 type, const u32 subtype)
{
	return zynqmp_pm_invoke_fn(PM_SYSTEM_SHUTDOWN, type, subtype,
				   0, 0, NULL);
}

 
int zynqmp_pm_set_feature_config(enum pm_feature_config_id id, u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_SET_FEATURE_CONFIG,
				   id, value, NULL);
}

 
int zynqmp_pm_get_feature_config(enum pm_feature_config_id id,
				 u32 *payload)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_GET_FEATURE_CONFIG,
				   id, 0, payload);
}

 
int zynqmp_pm_set_sd_config(u32 node, enum pm_sd_config_type config, u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, node, IOCTL_SET_SD_CONFIG,
				   config, value, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_sd_config);

 
int zynqmp_pm_set_gem_config(u32 node, enum pm_gem_config_type config,
			     u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, node, IOCTL_SET_GEM_CONFIG,
				   config, value, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_set_gem_config);

 
struct zynqmp_pm_shutdown_scope {
	const enum zynqmp_pm_shutdown_subtype subtype;
	const char *name;
};

static struct zynqmp_pm_shutdown_scope shutdown_scopes[] = {
	[ZYNQMP_PM_SHUTDOWN_SUBTYPE_SUBSYSTEM] = {
		.subtype = ZYNQMP_PM_SHUTDOWN_SUBTYPE_SUBSYSTEM,
		.name = "subsystem",
	},
	[ZYNQMP_PM_SHUTDOWN_SUBTYPE_PS_ONLY] = {
		.subtype = ZYNQMP_PM_SHUTDOWN_SUBTYPE_PS_ONLY,
		.name = "ps_only",
	},
	[ZYNQMP_PM_SHUTDOWN_SUBTYPE_SYSTEM] = {
		.subtype = ZYNQMP_PM_SHUTDOWN_SUBTYPE_SYSTEM,
		.name = "system",
	},
};

static struct zynqmp_pm_shutdown_scope *selected_scope =
		&shutdown_scopes[ZYNQMP_PM_SHUTDOWN_SUBTYPE_SYSTEM];

 
static struct zynqmp_pm_shutdown_scope*
		zynqmp_pm_is_shutdown_scope_valid(const char *scope_string)
{
	int count;

	for (count = 0; count < ARRAY_SIZE(shutdown_scopes); count++)
		if (sysfs_streq(scope_string, shutdown_scopes[count].name))
			return &shutdown_scopes[count];

	return NULL;
}

static ssize_t shutdown_scope_show(struct device *device,
				   struct device_attribute *attr,
				   char *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(shutdown_scopes); i++) {
		if (&shutdown_scopes[i] == selected_scope) {
			strcat(buf, "[");
			strcat(buf, shutdown_scopes[i].name);
			strcat(buf, "]");
		} else {
			strcat(buf, shutdown_scopes[i].name);
		}
		strcat(buf, " ");
	}
	strcat(buf, "\n");

	return strlen(buf);
}

static ssize_t shutdown_scope_store(struct device *device,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret;
	struct zynqmp_pm_shutdown_scope *scope;

	scope = zynqmp_pm_is_shutdown_scope_valid(buf);
	if (!scope)
		return -EINVAL;

	ret = zynqmp_pm_system_shutdown(ZYNQMP_PM_SHUTDOWN_TYPE_SETSCOPE_ONLY,
					scope->subtype);
	if (ret) {
		pr_err("unable to set shutdown scope %s\n", buf);
		return ret;
	}

	selected_scope = scope;

	return count;
}

static DEVICE_ATTR_RW(shutdown_scope);

static ssize_t health_status_store(struct device *device,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	unsigned int value;

	ret = kstrtouint(buf, 10, &value);
	if (ret)
		return ret;

	ret = zynqmp_pm_set_boot_health_status(value);
	if (ret) {
		dev_err(device, "unable to set healthy bit value to %u\n",
			value);
		return ret;
	}

	return count;
}

static DEVICE_ATTR_WO(health_status);

static ssize_t ggs_show(struct device *device,
			struct device_attribute *attr,
			char *buf,
			u32 reg)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	ret = zynqmp_pm_read_ggs(reg, ret_payload);
	if (ret)
		return ret;

	return sprintf(buf, "0x%x\n", ret_payload[1]);
}

static ssize_t ggs_store(struct device *device,
			 struct device_attribute *attr,
			 const char *buf, size_t count,
			 u32 reg)
{
	long value;
	int ret;

	if (reg >= GSS_NUM_REGS)
		return -EINVAL;

	ret = kstrtol(buf, 16, &value);
	if (ret) {
		count = -EFAULT;
		goto err;
	}

	ret = zynqmp_pm_write_ggs(reg, value);
	if (ret)
		count = -EFAULT;
err:
	return count;
}

 
#define GGS0_SHOW(N)						\
	ssize_t ggs##N##_show(struct device *device,		\
			      struct device_attribute *attr,	\
			      char *buf)			\
	{							\
		return ggs_show(device, attr, buf, N);		\
	}

static GGS0_SHOW(0);
static GGS0_SHOW(1);
static GGS0_SHOW(2);
static GGS0_SHOW(3);

 
#define GGS0_STORE(N)						\
	ssize_t ggs##N##_store(struct device *device,		\
			       struct device_attribute *attr,	\
			       const char *buf,			\
			       size_t count)			\
	{							\
		return ggs_store(device, attr, buf, count, N);	\
	}

static GGS0_STORE(0);
static GGS0_STORE(1);
static GGS0_STORE(2);
static GGS0_STORE(3);

static ssize_t pggs_show(struct device *device,
			 struct device_attribute *attr,
			 char *buf,
			 u32 reg)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];

	ret = zynqmp_pm_read_pggs(reg, ret_payload);
	if (ret)
		return ret;

	return sprintf(buf, "0x%x\n", ret_payload[1]);
}

static ssize_t pggs_store(struct device *device,
			  struct device_attribute *attr,
			  const char *buf, size_t count,
			  u32 reg)
{
	long value;
	int ret;

	if (reg >= GSS_NUM_REGS)
		return -EINVAL;

	ret = kstrtol(buf, 16, &value);
	if (ret) {
		count = -EFAULT;
		goto err;
	}

	ret = zynqmp_pm_write_pggs(reg, value);
	if (ret)
		count = -EFAULT;

err:
	return count;
}

#define PGGS0_SHOW(N)						\
	ssize_t pggs##N##_show(struct device *device,		\
			       struct device_attribute *attr,	\
			       char *buf)			\
	{							\
		return pggs_show(device, attr, buf, N);		\
	}

#define PGGS0_STORE(N)						\
	ssize_t pggs##N##_store(struct device *device,		\
				struct device_attribute *attr,	\
				const char *buf,		\
				size_t count)			\
	{							\
		return pggs_store(device, attr, buf, count, N);	\
	}

 
static PGGS0_SHOW(0);
static PGGS0_SHOW(1);
static PGGS0_SHOW(2);
static PGGS0_SHOW(3);

 
static PGGS0_STORE(0);
static PGGS0_STORE(1);
static PGGS0_STORE(2);
static PGGS0_STORE(3);

 
static DEVICE_ATTR_RW(ggs0);
static DEVICE_ATTR_RW(ggs1);
static DEVICE_ATTR_RW(ggs2);
static DEVICE_ATTR_RW(ggs3);

 
static DEVICE_ATTR_RW(pggs0);
static DEVICE_ATTR_RW(pggs1);
static DEVICE_ATTR_RW(pggs2);
static DEVICE_ATTR_RW(pggs3);

static ssize_t feature_config_id_show(struct device *device,
				      struct device_attribute *attr,
				      char *buf)
{
	struct zynqmp_devinfo *devinfo = dev_get_drvdata(device);

	return sysfs_emit(buf, "%d\n", devinfo->feature_conf_id);
}

static ssize_t feature_config_id_store(struct device *device,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	u32 config_id;
	int ret;
	struct zynqmp_devinfo *devinfo = dev_get_drvdata(device);

	if (!buf)
		return -EINVAL;

	ret = kstrtou32(buf, 10, &config_id);
	if (ret)
		return ret;

	devinfo->feature_conf_id = config_id;

	return count;
}

static DEVICE_ATTR_RW(feature_config_id);

static ssize_t feature_config_value_show(struct device *device,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret;
	u32 ret_payload[PAYLOAD_ARG_CNT];
	struct zynqmp_devinfo *devinfo = dev_get_drvdata(device);

	ret = zynqmp_pm_get_feature_config(devinfo->feature_conf_id,
					   ret_payload);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%d\n", ret_payload[1]);
}

static ssize_t feature_config_value_store(struct device *device,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	u32 value;
	int ret;
	struct zynqmp_devinfo *devinfo = dev_get_drvdata(device);

	if (!buf)
		return -EINVAL;

	ret = kstrtou32(buf, 10, &value);
	if (ret)
		return ret;

	ret = zynqmp_pm_set_feature_config(devinfo->feature_conf_id,
					   value);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(feature_config_value);

static struct attribute *zynqmp_firmware_attrs[] = {
	&dev_attr_ggs0.attr,
	&dev_attr_ggs1.attr,
	&dev_attr_ggs2.attr,
	&dev_attr_ggs3.attr,
	&dev_attr_pggs0.attr,
	&dev_attr_pggs1.attr,
	&dev_attr_pggs2.attr,
	&dev_attr_pggs3.attr,
	&dev_attr_shutdown_scope.attr,
	&dev_attr_health_status.attr,
	&dev_attr_feature_config_id.attr,
	&dev_attr_feature_config_value.attr,
	NULL,
};

ATTRIBUTE_GROUPS(zynqmp_firmware);

static int zynqmp_firmware_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct zynqmp_devinfo *devinfo;
	int ret;

	ret = get_set_conduit_method(dev->of_node);
	if (ret)
		return ret;

	np = of_find_compatible_node(NULL, NULL, "xlnx,zynqmp");
	if (!np) {
		np = of_find_compatible_node(NULL, NULL, "xlnx,versal");
		if (!np)
			return 0;

		feature_check_enabled = true;
	}

	if (!feature_check_enabled) {
		ret = do_feature_check_call(PM_FEATURE_CHECK);
		if (ret >= 0)
			feature_check_enabled = true;
	}

	of_node_put(np);

	devinfo = devm_kzalloc(dev, sizeof(*devinfo), GFP_KERNEL);
	if (!devinfo)
		return -ENOMEM;

	devinfo->dev = dev;

	platform_set_drvdata(pdev, devinfo);

	 
	ret = zynqmp_pm_get_api_version(&pm_api_version);
	if (ret)
		return ret;

	if (pm_api_version < ZYNQMP_PM_VERSION) {
		panic("%s Platform Management API version error. Expected: v%d.%d - Found: v%d.%d\n",
		      __func__,
		      ZYNQMP_PM_VERSION_MAJOR, ZYNQMP_PM_VERSION_MINOR,
		      pm_api_version >> 16, pm_api_version & 0xFFFF);
	}

	pr_info("%s Platform Management API v%d.%d\n", __func__,
		pm_api_version >> 16, pm_api_version & 0xFFFF);

	 
	ret = zynqmp_pm_get_family_info(&pm_family_code, &pm_sub_family_code);
	if (ret < 0)
		return ret;

	 
	ret = zynqmp_pm_get_trustzone_version(&pm_tz_version);
	if (ret)
		panic("Legacy trustzone found without version support\n");

	if (pm_tz_version < ZYNQMP_TZ_VERSION)
		panic("%s Trustzone version error. Expected: v%d.%d - Found: v%d.%d\n",
		      __func__,
		      ZYNQMP_TZ_VERSION_MAJOR, ZYNQMP_TZ_VERSION_MINOR,
		      pm_tz_version >> 16, pm_tz_version & 0xFFFF);

	pr_info("%s Trustzone version v%d.%d\n", __func__,
		pm_tz_version >> 16, pm_tz_version & 0xFFFF);

	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE, firmware_devs,
			      ARRAY_SIZE(firmware_devs), NULL, 0, NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to add MFD devices %d\n", ret);
		return ret;
	}

	zynqmp_pm_api_debugfs_init();

	np = of_find_compatible_node(NULL, NULL, "xlnx,versal");
	if (np) {
		em_dev = platform_device_register_data(&pdev->dev, "xlnx_event_manager",
						       -1, NULL, 0);
		if (IS_ERR(em_dev))
			dev_err_probe(&pdev->dev, PTR_ERR(em_dev), "EM register fail with error\n");
	}
	of_node_put(np);

	return of_platform_populate(dev->of_node, NULL, NULL, dev);
}

static int zynqmp_firmware_remove(struct platform_device *pdev)
{
	struct pm_api_feature_data *feature_data;
	struct hlist_node *tmp;
	int i;

	mfd_remove_devices(&pdev->dev);
	zynqmp_pm_api_debugfs_exit();

	hash_for_each_safe(pm_api_features_map, i, tmp, feature_data, hentry) {
		hash_del(&feature_data->hentry);
		kfree(feature_data);
	}

	platform_device_unregister(em_dev);

	return 0;
}

static const struct of_device_id zynqmp_firmware_of_match[] = {
	{.compatible = "xlnx,zynqmp-firmware"},
	{.compatible = "xlnx,versal-firmware"},
	{},
};
MODULE_DEVICE_TABLE(of, zynqmp_firmware_of_match);

static struct platform_driver zynqmp_firmware_driver = {
	.driver = {
		.name = "zynqmp_firmware",
		.of_match_table = zynqmp_firmware_of_match,
		.dev_groups = zynqmp_firmware_groups,
	},
	.probe = zynqmp_firmware_probe,
	.remove = zynqmp_firmware_remove,
};
module_platform_driver(zynqmp_firmware_driver);
