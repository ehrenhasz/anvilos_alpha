#ifndef _KOMEDA_DEV_H_
#define _KOMEDA_DEV_H_
#include <linux/device.h>
#include <linux/clk.h>
#include "komeda_pipeline.h"
#include "malidp_product.h"
#include "komeda_format_caps.h"
#define KOMEDA_EVENT_VSYNC		BIT_ULL(0)
#define KOMEDA_EVENT_FLIP		BIT_ULL(1)
#define KOMEDA_EVENT_URUN		BIT_ULL(2)
#define KOMEDA_EVENT_IBSY		BIT_ULL(3)
#define KOMEDA_EVENT_OVR		BIT_ULL(4)
#define KOMEDA_EVENT_EOW		BIT_ULL(5)
#define KOMEDA_EVENT_MODE		BIT_ULL(6)
#define KOMEDA_EVENT_FULL		BIT_ULL(7)
#define KOMEDA_EVENT_EMPTY		BIT_ULL(8)
#define KOMEDA_ERR_TETO			BIT_ULL(14)
#define KOMEDA_ERR_TEMR			BIT_ULL(15)
#define KOMEDA_ERR_TITR			BIT_ULL(16)
#define KOMEDA_ERR_CPE			BIT_ULL(17)
#define KOMEDA_ERR_CFGE			BIT_ULL(18)
#define KOMEDA_ERR_AXIE			BIT_ULL(19)
#define KOMEDA_ERR_ACE0			BIT_ULL(20)
#define KOMEDA_ERR_ACE1			BIT_ULL(21)
#define KOMEDA_ERR_ACE2			BIT_ULL(22)
#define KOMEDA_ERR_ACE3			BIT_ULL(23)
#define KOMEDA_ERR_DRIFTTO		BIT_ULL(24)
#define KOMEDA_ERR_FRAMETO		BIT_ULL(25)
#define KOMEDA_ERR_CSCE			BIT_ULL(26)
#define KOMEDA_ERR_ZME			BIT_ULL(27)
#define KOMEDA_ERR_MERR			BIT_ULL(28)
#define KOMEDA_ERR_TCF			BIT_ULL(29)
#define KOMEDA_ERR_TTNG			BIT_ULL(30)
#define KOMEDA_ERR_TTF			BIT_ULL(31)
#define KOMEDA_ERR_EVENTS	\
	(KOMEDA_EVENT_URUN	| KOMEDA_EVENT_IBSY	| KOMEDA_EVENT_OVR |\
	KOMEDA_ERR_TETO		| KOMEDA_ERR_TEMR	| KOMEDA_ERR_TITR |\
	KOMEDA_ERR_CPE		| KOMEDA_ERR_CFGE	| KOMEDA_ERR_AXIE |\
	KOMEDA_ERR_ACE0		| KOMEDA_ERR_ACE1	| KOMEDA_ERR_ACE2 |\
	KOMEDA_ERR_ACE3		| KOMEDA_ERR_DRIFTTO	| KOMEDA_ERR_FRAMETO |\
	KOMEDA_ERR_ZME		| KOMEDA_ERR_MERR	| KOMEDA_ERR_TCF |\
	KOMEDA_ERR_TTNG		| KOMEDA_ERR_TTF)
#define KOMEDA_WARN_EVENTS	\
	(KOMEDA_ERR_CSCE | KOMEDA_EVENT_FULL | KOMEDA_EVENT_EMPTY)
#define KOMEDA_INFO_EVENTS (0 \
			    | KOMEDA_EVENT_VSYNC \
			    | KOMEDA_EVENT_FLIP \
			    | KOMEDA_EVENT_EOW \
			    | KOMEDA_EVENT_MODE \
			    )
enum {
	KOMEDA_OF_PORT_OUTPUT		= 0,
	KOMEDA_OF_PORT_COPROC		= 1,
};
struct komeda_chip_info {
	u32 arch_id;
	u32 core_id;
	u32 core_info;
	u32 bus_width;
};
struct komeda_dev;
struct komeda_events {
	u64 global;
	u64 pipes[KOMEDA_MAX_PIPELINES];
};
struct komeda_dev_funcs {
	void (*init_format_table)(struct komeda_dev *mdev);
	int (*enum_resources)(struct komeda_dev *mdev);
	void (*cleanup)(struct komeda_dev *mdev);
	int (*connect_iommu)(struct komeda_dev *mdev);
	int (*disconnect_iommu)(struct komeda_dev *mdev);
	irqreturn_t (*irq_handler)(struct komeda_dev *mdev,
				   struct komeda_events *events);
	int (*enable_irq)(struct komeda_dev *mdev);
	int (*disable_irq)(struct komeda_dev *mdev);
	void (*on_off_vblank)(struct komeda_dev *mdev,
			      int master_pipe, bool on);
	void (*dump_register)(struct komeda_dev *mdev, struct seq_file *seq);
	int (*change_opmode)(struct komeda_dev *mdev, int new_mode);
	void (*flush)(struct komeda_dev *mdev,
		      int master_pipe, u32 active_pipes);
};
enum {
	KOMEDA_MODE_INACTIVE	= 0,
	KOMEDA_MODE_DISP0	= BIT(0),
	KOMEDA_MODE_DISP1	= BIT(1),
	KOMEDA_MODE_DUAL_DISP	= KOMEDA_MODE_DISP0 | KOMEDA_MODE_DISP1,
};
struct komeda_dev {
	struct device *dev;
	u32 __iomem   *reg_base;
	struct komeda_chip_info chip;
	struct komeda_format_caps_table fmt_tbl;
	struct clk *aclk;
	int irq;
	struct mutex lock;
	u32 dpmode;
	int n_pipelines;
	struct komeda_pipeline *pipelines[KOMEDA_MAX_PIPELINES];
	const struct komeda_dev_funcs *funcs;
	void *chip_data;
	struct iommu_domain *iommu;
	struct dentry *debugfs_root;
	u16 err_verbosity;
#define KOMEDA_DEV_PRINT_ERR_EVENTS BIT(0)
#define KOMEDA_DEV_PRINT_WARN_EVENTS BIT(1)
#define KOMEDA_DEV_PRINT_INFO_EVENTS BIT(2)
#define KOMEDA_DEV_PRINT_DUMP_STATE_ON_EVENT BIT(8)
#define KOMEDA_DEV_PRINT_DISABLE_RATELIMIT BIT(12)
};
static inline bool
komeda_product_match(struct komeda_dev *mdev, u32 target)
{
	return MALIDP_CORE_ID_PRODUCT_ID(mdev->chip.core_id) == target;
}
typedef const struct komeda_dev_funcs *
(*komeda_identify_func)(u32 __iomem *reg, struct komeda_chip_info *chip);
const struct komeda_dev_funcs *
d71_identify(u32 __iomem *reg, struct komeda_chip_info *chip);
struct komeda_dev *komeda_dev_create(struct device *dev);
void komeda_dev_destroy(struct komeda_dev *mdev);
struct komeda_dev *dev_to_mdev(struct device *dev);
void komeda_print_events(struct komeda_events *evts, struct drm_device *dev);
int komeda_dev_resume(struct komeda_dev *mdev);
int komeda_dev_suspend(struct komeda_dev *mdev);
#endif  
