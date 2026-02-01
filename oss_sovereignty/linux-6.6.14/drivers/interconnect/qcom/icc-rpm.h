 
 

#ifndef __DRIVERS_INTERCONNECT_QCOM_ICC_RPM_H
#define __DRIVERS_INTERCONNECT_QCOM_ICC_RPM_H

#include <linux/soc/qcom/smd-rpm.h>

#include <dt-bindings/interconnect/qcom,rpm-icc.h>
#include <linux/clk.h>
#include <linux/interconnect-provider.h>
#include <linux/platform_device.h>

#define RPM_BUS_MASTER_REQ	0x73616d62
#define RPM_BUS_SLAVE_REQ	0x766c7362

#define to_qcom_provider(_provider) \
	container_of(_provider, struct qcom_icc_provider, provider)

enum qcom_icc_type {
	QCOM_ICC_NOC,
	QCOM_ICC_BIMC,
	QCOM_ICC_QNOC,
};

 
struct rpm_clk_resource {
	u32 resource_type;
	u32 clock_id;
	bool branch;
};

 
struct qcom_icc_provider {
	struct icc_provider provider;
	int num_intf_clks;
	enum qcom_icc_type type;
	struct regmap *regmap;
	unsigned int qos_offset;
	u32 bus_clk_rate[QCOM_SMD_RPM_STATE_NUM];
	const struct rpm_clk_resource *bus_clk_desc;
	struct clk *bus_clk;
	struct clk_bulk_data *intf_clks;
	bool keep_alive;
	bool is_on;
};

 
struct qcom_icc_qos {
	u32 areq_prio;
	u32 prio_level;
	bool limit_commands;
	bool ap_owned;
	int qos_mode;
	int qos_port;
	bool urg_fwd_en;
};

 
struct qcom_icc_node {
	unsigned char *name;
	u16 id;
	const u16 *links;
	u16 num_links;
	u16 channels;
	u16 buswidth;
	u64 sum_avg[QCOM_SMD_RPM_STATE_NUM];
	u64 max_peak[QCOM_SMD_RPM_STATE_NUM];
	int mas_rpm_id;
	int slv_rpm_id;
	struct qcom_icc_qos qos;
};

struct qcom_icc_desc {
	struct qcom_icc_node * const *nodes;
	size_t num_nodes;
	const struct rpm_clk_resource *bus_clk_desc;
	const char * const *intf_clocks;
	size_t num_intf_clocks;
	bool keep_alive;
	enum qcom_icc_type type;
	const struct regmap_config *regmap_cfg;
	unsigned int qos_offset;
};

 
enum qos_mode {
	NOC_QOS_MODE_INVALID = 0,
	NOC_QOS_MODE_FIXED,
	NOC_QOS_MODE_BYPASS,
};

extern const struct rpm_clk_resource aggre1_clk;
extern const struct rpm_clk_resource aggre2_clk;
extern const struct rpm_clk_resource bimc_clk;
extern const struct rpm_clk_resource bus_0_clk;
extern const struct rpm_clk_resource bus_1_clk;
extern const struct rpm_clk_resource bus_2_clk;
extern const struct rpm_clk_resource mmaxi_0_clk;
extern const struct rpm_clk_resource mmaxi_1_clk;
extern const struct rpm_clk_resource qup_clk;

extern const struct rpm_clk_resource aggre1_branch_clk;
extern const struct rpm_clk_resource aggre2_branch_clk;

int qnoc_probe(struct platform_device *pdev);
int qnoc_remove(struct platform_device *pdev);

bool qcom_icc_rpm_smd_available(void);
int qcom_icc_rpm_smd_send(int ctx, int rsc_type, int id, u32 val);
int qcom_icc_rpm_set_bus_rate(const struct rpm_clk_resource *clk, int ctx, u32 rate);

#endif
