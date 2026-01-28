
#ifndef _ISCI_PROBE_ROMS_H_
#define _ISCI_PROBE_ROMS_H_

#ifdef __KERNEL__
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/efi.h>
#include "isci.h"

#define SCIC_SDS_PARM_NO_SPEED   0


#define SCIC_SDS_PARM_GEN1_SPEED 1


#define SCIC_SDS_PARM_GEN2_SPEED 2


#define SCIC_SDS_PARM_GEN3_SPEED 3
#define SCIC_SDS_PARM_MAX_SPEED SCIC_SDS_PARM_GEN3_SPEED


struct sci_user_parameters {
	struct sci_phy_user_params {
		
		u32 notify_enable_spin_up_insertion_frequency;

		
		u16 align_insertion_frequency;

		
		u16 in_connection_align_insertion_frequency;

		
		u8 max_speed_generation;

	} phys[SCI_MAX_PHYS];

	
	u8 max_concurr_spinup;

	
	u8 phy_spin_up_delay_interval;

	
	u16 stp_inactivity_timeout;
	u16 ssp_inactivity_timeout;

	
	u16 stp_max_occupancy_timeout;
	u16 ssp_max_occupancy_timeout;

	
	u8 no_outbound_task_timeout;

};

#define SCIC_SDS_PARM_PHY_MASK_MIN 0x0
#define SCIC_SDS_PARM_PHY_MASK_MAX 0xF
#define MAX_CONCURRENT_DEVICE_SPIN_UP_COUNT 4

struct sci_oem_params;
int sci_oem_parameters_validate(struct sci_oem_params *oem, u8 version);

struct isci_orom;
struct isci_orom *isci_request_oprom(struct pci_dev *pdev);
struct isci_orom *isci_request_firmware(struct pci_dev *pdev, const struct firmware *fw);
struct isci_orom *isci_get_efi_var(struct pci_dev *pdev);

struct isci_oem_hdr {
	u8 sig[4];
	u8 rev_major;
	u8 rev_minor;
	u16 len;
	u8 checksum;
	u8 reserved1;
	u16 reserved2;
} __attribute__ ((packed));

#else
#define SCI_MAX_PORTS 4
#define SCI_MAX_PHYS 4
#define SCI_MAX_CONTROLLERS 2
#endif

#define ISCI_FW_NAME		"isci/isci_firmware.bin"

#define ROMSIGNATURE		0xaa55

#define ISCI_OEM_SIG		"$OEM"
#define ISCI_OEM_SIG_SIZE	4
#define ISCI_ROM_SIG		"ISCUOEMB"
#define ISCI_ROM_SIG_SIZE	8

#define ISCI_EFI_VENDOR_GUID	\
	EFI_GUID(0x193dfefa, 0xa445, 0x4302, 0x99, 0xd8, 0xef, 0x3a, 0xad, \
			0x1a, 0x04, 0xc6)
#define ISCI_EFI_VAR_NAME	"RstScuO"

#define ISCI_ROM_VER_1_0	0x10
#define ISCI_ROM_VER_1_1	0x11
#define ISCI_ROM_VER_1_3	0x13
#define ISCI_ROM_VER_LATEST	ISCI_ROM_VER_1_3


enum sci_port_configuration_mode {
	SCIC_PORT_MANUAL_CONFIGURATION_MODE = 0,
	SCIC_PORT_AUTOMATIC_CONFIGURATION_MODE = 1
};

struct sci_bios_oem_param_block_hdr {
	uint8_t signature[ISCI_ROM_SIG_SIZE];
	uint16_t total_block_length;
	uint8_t hdr_length;
	uint8_t version;
	uint8_t preboot_source;
	uint8_t num_elements;
	uint16_t element_length;
	uint8_t reserved[8];
} __attribute__ ((packed));

struct sci_oem_params {
	struct {
		uint8_t mode_type;
		uint8_t max_concurr_spin_up;
		
		union {
			struct {
			
				uint8_t ssc_sata_tx_spread_level:4;
			
				uint8_t ssc_sas_tx_spread_level:3;
			
				uint8_t ssc_sas_tx_type:1;
			};
			uint8_t do_enable_ssc;
		};
		
		uint8_t cable_selection_mask;
	} controller;

	struct {
		uint8_t phy_mask;
	} ports[SCI_MAX_PORTS];

	struct sci_phy_oem_params {
		struct {
			uint32_t high;
			uint32_t low;
		} sas_address;

		uint32_t afe_tx_amp_control0;
		uint32_t afe_tx_amp_control1;
		uint32_t afe_tx_amp_control2;
		uint32_t afe_tx_amp_control3;
	} phys[SCI_MAX_PHYS];
} __attribute__ ((packed));

struct isci_orom {
	struct sci_bios_oem_param_block_hdr hdr;
	struct sci_oem_params ctrl[SCI_MAX_CONTROLLERS];
} __attribute__ ((packed));

#endif
