#ifndef __NFC_S3FWRN5_PHY_COMMON_H
#define __NFC_S3FWRN5_PHY_COMMON_H
#include <linux/mutex.h>
#include <net/nfc/nci_core.h>
#include "s3fwrn5.h"
#define S3FWRN5_EN_WAIT_TIME 20
struct phy_common {
	struct nci_dev *ndev;
	int gpio_en;
	int gpio_fw_wake;
	struct mutex mutex;
	enum s3fwrn5_mode mode;
};
void s3fwrn5_phy_set_wake(void *phy_id, bool wake);
bool s3fwrn5_phy_power_ctrl(struct phy_common *phy, enum s3fwrn5_mode mode);
void s3fwrn5_phy_set_mode(void *phy_id, enum s3fwrn5_mode mode);
enum s3fwrn5_mode s3fwrn5_phy_get_mode(void *phy_id);
#endif  
