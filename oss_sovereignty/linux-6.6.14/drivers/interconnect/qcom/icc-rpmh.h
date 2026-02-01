 
 

#ifndef __DRIVERS_INTERCONNECT_QCOM_ICC_RPMH_H__
#define __DRIVERS_INTERCONNECT_QCOM_ICC_RPMH_H__

#include <dt-bindings/interconnect/qcom,icc.h>

#define to_qcom_provider(_provider) \
	container_of(_provider, struct qcom_icc_provider, provider)

 
struct qcom_icc_provider {
	struct icc_provider provider;
	struct device *dev;
	struct qcom_icc_bcm * const *bcms;
	size_t num_bcms;
	struct bcm_voter *voter;
};

 
struct bcm_db {
	__le32 unit;
	__le16 width;
	u8 vcd;
	u8 reserved;
};

#define MAX_LINKS		128
#define MAX_BCMS		64
#define MAX_BCM_PER_NODE	3
#define MAX_VCD			10

 
struct qcom_icc_node {
	const char *name;
	u16 links[MAX_LINKS];
	u16 id;
	u16 num_links;
	u16 channels;
	u16 buswidth;
	u64 sum_avg[QCOM_ICC_NUM_BUCKETS];
	u64 max_peak[QCOM_ICC_NUM_BUCKETS];
	struct qcom_icc_bcm *bcms[MAX_BCM_PER_NODE];
	size_t num_bcms;
};

 
struct qcom_icc_bcm {
	const char *name;
	u32 type;
	u32 addr;
	u64 vote_x[QCOM_ICC_NUM_BUCKETS];
	u64 vote_y[QCOM_ICC_NUM_BUCKETS];
	u64 vote_scale;
	u32 enable_mask;
	bool dirty;
	bool keepalive;
	struct bcm_db aux_data;
	struct list_head list;
	struct list_head ws_list;
	size_t num_nodes;
	struct qcom_icc_node *nodes[];
};

struct qcom_icc_fabric {
	struct qcom_icc_node **nodes;
	size_t num_nodes;
};

struct qcom_icc_desc {
	struct qcom_icc_node * const *nodes;
	size_t num_nodes;
	struct qcom_icc_bcm * const *bcms;
	size_t num_bcms;
};

int qcom_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
		       u32 peak_bw, u32 *agg_avg, u32 *agg_peak);
int qcom_icc_set(struct icc_node *src, struct icc_node *dst);
int qcom_icc_bcm_init(struct qcom_icc_bcm *bcm, struct device *dev);
void qcom_icc_pre_aggregate(struct icc_node *node);
int qcom_icc_rpmh_probe(struct platform_device *pdev);
int qcom_icc_rpmh_remove(struct platform_device *pdev);

#endif
