


#include <linux/device.h>
#include <linux/firmware.h>

struct rtl8169_private;
typedef void (*rtl_fw_write_t)(struct rtl8169_private *tp, int reg, int val);
typedef int (*rtl_fw_read_t)(struct rtl8169_private *tp, int reg);

#define RTL_VER_SIZE		32

struct rtl_fw {
	rtl_fw_write_t phy_write;
	rtl_fw_read_t phy_read;
	rtl_fw_write_t mac_mcu_write;
	rtl_fw_read_t mac_mcu_read;
	const struct firmware *fw;
	const char *fw_name;
	struct device *dev;

	char version[RTL_VER_SIZE];

	struct rtl_fw_phy_action {
		__le32 *code;
		size_t size;
	} phy_action;
};

int rtl_fw_request_firmware(struct rtl_fw *rtl_fw);
void rtl_fw_release_firmware(struct rtl_fw *rtl_fw);
void rtl_fw_write_firmware(struct rtl8169_private *tp, struct rtl_fw *rtl_fw);
