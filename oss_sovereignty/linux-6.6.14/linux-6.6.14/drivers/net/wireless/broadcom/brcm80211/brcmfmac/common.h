#ifndef BRCMFMAC_COMMON_H
#define BRCMFMAC_COMMON_H
#include <linux/platform_device.h>
#include <linux/platform_data/brcmfmac.h>
#include "fwil_types.h"
#define BRCMF_FW_ALTPATH_LEN			256
struct brcmf_mp_global_t {
	char	firmware_path[BRCMF_FW_ALTPATH_LEN];
};
extern struct brcmf_mp_global_t brcmf_mp_global;
struct brcmf_mp_device {
	bool		p2p_enable;
	unsigned int	feature_disable;
	int		fcmode;
	bool		roamoff;
	bool		iapp;
	bool		ignore_probe_fail;
	bool		trivial_ccode_map;
	struct brcmfmac_pd_cc *country_codes;
	const char	*board_type;
	unsigned char	mac[ETH_ALEN];
	const char	*antenna_sku;
	const void	*cal_blob;
	int		cal_size;
	union {
		struct brcmfmac_sdio_pd sdio;
	} bus;
};
void brcmf_c_set_joinpref_default(struct brcmf_if *ifp);
struct brcmf_mp_device *brcmf_get_module_param(struct device *dev,
					       enum brcmf_bus_type bus_type,
					       u32 chip, u32 chiprev);
void brcmf_release_module_param(struct brcmf_mp_device *module_param);
int brcmf_c_preinit_dcmds(struct brcmf_if *ifp);
int brcmf_c_set_cur_etheraddr(struct brcmf_if *ifp, const u8 *addr);
#ifdef CONFIG_DMI
void brcmf_dmi_probe(struct brcmf_mp_device *settings, u32 chip, u32 chiprev);
#else
static inline void
brcmf_dmi_probe(struct brcmf_mp_device *settings, u32 chip, u32 chiprev) {}
#endif
#ifdef CONFIG_ACPI
void brcmf_acpi_probe(struct device *dev, enum brcmf_bus_type bus_type,
		      struct brcmf_mp_device *settings);
#else
static inline void brcmf_acpi_probe(struct device *dev,
				    enum brcmf_bus_type bus_type,
				    struct brcmf_mp_device *settings) {}
#endif
u8 brcmf_map_prio_to_prec(void *cfg, u8 prio);
u8 brcmf_map_prio_to_aci(void *cfg, u8 prio);
#endif  
