 
 

#ifndef NSP_NSP_H
#define NSP_NSP_H 1

#include <linux/types.h>
#include <linux/if_ether.h>

struct firmware;
struct nfp_cpp;
struct nfp_nsp;

struct nfp_nsp *nfp_nsp_open(struct nfp_cpp *cpp);
void nfp_nsp_close(struct nfp_nsp *state);
u16 nfp_nsp_get_abi_ver_major(struct nfp_nsp *state);
u16 nfp_nsp_get_abi_ver_minor(struct nfp_nsp *state);
int nfp_nsp_wait(struct nfp_nsp *state);
int nfp_nsp_device_soft_reset(struct nfp_nsp *state);
int nfp_nsp_load_fw(struct nfp_nsp *state, const struct firmware *fw);
int nfp_nsp_write_flash(struct nfp_nsp *state, const struct firmware *fw);
int nfp_nsp_mac_reinit(struct nfp_nsp *state);
int nfp_nsp_load_stored_fw(struct nfp_nsp *state);
int nfp_nsp_hwinfo_lookup(struct nfp_nsp *state, void *buf, unsigned int size);
int nfp_nsp_hwinfo_lookup_optional(struct nfp_nsp *state, void *buf,
				   unsigned int size, const char *default_val);
int nfp_nsp_hwinfo_set(struct nfp_nsp *state, void *buf, unsigned int size);
int nfp_nsp_fw_loaded(struct nfp_nsp *state);
int nfp_nsp_read_module_eeprom(struct nfp_nsp *state, int eth_index,
			       unsigned int offset, void *data,
			       unsigned int len, unsigned int *read_len);

static inline bool nfp_nsp_has_mac_reinit(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 20;
}

static inline bool nfp_nsp_has_stored_fw_load(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 23;
}

static inline bool nfp_nsp_has_hwinfo_lookup(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 24;
}

static inline bool nfp_nsp_has_hwinfo_set(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 25;
}

static inline bool nfp_nsp_has_fw_loaded(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 25;
}

static inline bool nfp_nsp_has_versions(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 27;
}

static inline bool nfp_nsp_has_read_module_eeprom(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 28;
}

static inline bool nfp_nsp_has_read_media(struct nfp_nsp *state)
{
	return nfp_nsp_get_abi_ver_minor(state) > 33;
}

enum nfp_eth_interface {
	NFP_INTERFACE_NONE	= 0,
	NFP_INTERFACE_SFP	= 1,
	NFP_INTERFACE_SFPP	= 10,
	NFP_INTERFACE_SFP28	= 28,
	NFP_INTERFACE_QSFP	= 40,
	NFP_INTERFACE_RJ45	= 45,
	NFP_INTERFACE_CXP	= 100,
	NFP_INTERFACE_QSFP28	= 112,
};

enum nfp_eth_media {
	NFP_MEDIA_DAC_PASSIVE = 0,
	NFP_MEDIA_DAC_ACTIVE,
	NFP_MEDIA_FIBRE,
};

enum nfp_eth_aneg {
	NFP_ANEG_AUTO = 0,
	NFP_ANEG_SEARCH,
	NFP_ANEG_25G_CONSORTIUM,
	NFP_ANEG_25G_IEEE,
	NFP_ANEG_DISABLED,
};

enum nfp_eth_fec {
	NFP_FEC_AUTO_BIT = 0,
	NFP_FEC_BASER_BIT,
	NFP_FEC_REED_SOLOMON_BIT,
	NFP_FEC_DISABLED_BIT,
};

 
enum nfp_ethtool_link_mode_list {
	NFP_MEDIA_W0_RJ45_10M,
	NFP_MEDIA_W0_RJ45_10M_HD,
	NFP_MEDIA_W0_RJ45_100M,
	NFP_MEDIA_W0_RJ45_100M_HD,
	NFP_MEDIA_W0_RJ45_1G,
	NFP_MEDIA_W0_RJ45_2P5G,
	NFP_MEDIA_W0_RJ45_5G,
	NFP_MEDIA_W0_RJ45_10G,
	NFP_MEDIA_1000BASE_CX,
	NFP_MEDIA_1000BASE_KX,
	NFP_MEDIA_10GBASE_KX4,
	NFP_MEDIA_10GBASE_KR,
	NFP_MEDIA_10GBASE_CX4,
	NFP_MEDIA_10GBASE_CR,
	NFP_MEDIA_10GBASE_SR,
	NFP_MEDIA_10GBASE_ER,
	NFP_MEDIA_25GBASE_KR,
	NFP_MEDIA_25GBASE_KR_S,
	NFP_MEDIA_25GBASE_CR,
	NFP_MEDIA_25GBASE_CR_S,
	NFP_MEDIA_25GBASE_SR,
	NFP_MEDIA_40GBASE_CR4,
	NFP_MEDIA_40GBASE_KR4,
	NFP_MEDIA_40GBASE_SR4,
	NFP_MEDIA_40GBASE_LR4,
	NFP_MEDIA_50GBASE_KR,
	NFP_MEDIA_50GBASE_SR,
	NFP_MEDIA_50GBASE_CR,
	NFP_MEDIA_50GBASE_LR,
	NFP_MEDIA_50GBASE_ER,
	NFP_MEDIA_50GBASE_FR,
	NFP_MEDIA_100GBASE_KR4,
	NFP_MEDIA_100GBASE_SR4,
	NFP_MEDIA_100GBASE_CR4,
	NFP_MEDIA_100GBASE_KP4,
	NFP_MEDIA_100GBASE_CR10,
	NFP_MEDIA_10GBASE_LR,
	NFP_MEDIA_25GBASE_LR,
	NFP_MEDIA_25GBASE_ER,
	NFP_MEDIA_LINK_MODES_NUMBER
};

#define NFP_FEC_AUTO		BIT(NFP_FEC_AUTO_BIT)
#define NFP_FEC_BASER		BIT(NFP_FEC_BASER_BIT)
#define NFP_FEC_REED_SOLOMON	BIT(NFP_FEC_REED_SOLOMON_BIT)
#define NFP_FEC_DISABLED	BIT(NFP_FEC_DISABLED_BIT)

 
#define NFP_NSP_DRV_RESET_DISK			0
#define NFP_NSP_DRV_RESET_ALWAYS		1
#define NFP_NSP_DRV_RESET_NEVER			2
#define NFP_NSP_DRV_RESET_DEFAULT		"0"

 
#define NFP_NSP_APP_FW_LOAD_DISK		0
#define NFP_NSP_APP_FW_LOAD_FLASH		1
#define NFP_NSP_APP_FW_LOAD_PREF		2
#define NFP_NSP_APP_FW_LOAD_DEFAULT		"2"

 
#define NFP_NSP_DRV_LOAD_IFC_DEFAULT		"0x10ff"

 
struct nfp_eth_table {
	unsigned int count;
	unsigned int max_index;
	struct nfp_eth_table_port {
		unsigned int eth_index;
		unsigned int index;
		unsigned int nbi;
		unsigned int base;
		unsigned int lanes;
		unsigned int speed;

		unsigned int interface;
		enum nfp_eth_media media;

		enum nfp_eth_fec fec;
		enum nfp_eth_fec act_fec;
		enum nfp_eth_aneg aneg;

		u8 mac_addr[ETH_ALEN];

		u8 label_port;
		u8 label_subport;

		bool enabled;
		bool tx_enabled;
		bool rx_enabled;
		bool supp_aneg;

		bool override_changed;

		 
		u8 port_type;

		unsigned int port_lanes;

		bool is_split;

		unsigned int fec_modes_supported;

		u64 link_modes_supp[2];
		u64 link_modes_ad[2];
	} ports[];
};

struct nfp_eth_table *nfp_eth_read_ports(struct nfp_cpp *cpp);
struct nfp_eth_table *
__nfp_eth_read_ports(struct nfp_cpp *cpp, struct nfp_nsp *nsp);

int nfp_eth_set_mod_enable(struct nfp_cpp *cpp, unsigned int idx, bool enable);
int nfp_eth_set_configured(struct nfp_cpp *cpp, unsigned int idx,
			   bool configed);
int
nfp_eth_set_fec(struct nfp_cpp *cpp, unsigned int idx, enum nfp_eth_fec mode);

int nfp_eth_set_idmode(struct nfp_cpp *cpp, unsigned int idx, bool state);

static inline bool nfp_eth_can_support_fec(struct nfp_eth_table_port *eth_port)
{
	return !!eth_port->fec_modes_supported;
}

static inline unsigned int
nfp_eth_supported_fec_modes(struct nfp_eth_table_port *eth_port)
{
	return eth_port->fec_modes_supported;
}

struct nfp_nsp *nfp_eth_config_start(struct nfp_cpp *cpp, unsigned int idx);
int nfp_eth_config_commit_end(struct nfp_nsp *nsp);
void nfp_eth_config_cleanup_end(struct nfp_nsp *nsp);

int __nfp_eth_set_aneg(struct nfp_nsp *nsp, enum nfp_eth_aneg mode);
int __nfp_eth_set_speed(struct nfp_nsp *nsp, unsigned int speed);
int __nfp_eth_set_split(struct nfp_nsp *nsp, unsigned int lanes);

 
struct nfp_nsp_identify {
	char version[40];
	u8 flags;
	u8 br_primary;
	u8 br_secondary;
	u8 br_nsp;
	u16 primary;
	u16 secondary;
	u16 nsp;
	u64 sensor_mask;
};

struct nfp_nsp_identify *__nfp_nsp_identify(struct nfp_nsp *nsp);

enum nfp_nsp_sensor_id {
	NFP_SENSOR_CHIP_TEMPERATURE,
	NFP_SENSOR_ASSEMBLY_POWER,
	NFP_SENSOR_ASSEMBLY_12V_POWER,
	NFP_SENSOR_ASSEMBLY_3V3_POWER,
};

int nfp_hwmon_read_sensor(struct nfp_cpp *cpp, enum nfp_nsp_sensor_id id,
			  long *val);

struct nfp_eth_media_buf {
	u8 eth_index;
	u8 reserved[7];
	__le64 supported_modes[2];
	__le64 advertised_modes[2];
};

int nfp_nsp_read_media(struct nfp_nsp *state, void *buf, unsigned int size);

#define NFP_NSP_VERSION_BUFSZ	1024  

enum nfp_nsp_versions {
	NFP_VERSIONS_BSP,
	NFP_VERSIONS_CPLD,
	NFP_VERSIONS_APP,
	NFP_VERSIONS_BUNDLE,
	NFP_VERSIONS_UNDI,
	NFP_VERSIONS_NCSI,
	NFP_VERSIONS_CFGR,
};

int nfp_nsp_versions(struct nfp_nsp *state, void *buf, unsigned int size);
const char *nfp_nsp_versions_get(enum nfp_nsp_versions id, bool flash,
				 const u8 *buf, unsigned int size);
#endif
