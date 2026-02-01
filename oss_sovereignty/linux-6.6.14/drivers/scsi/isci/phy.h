 
#ifndef _ISCI_PHY_H_
#define _ISCI_PHY_H_

#include <scsi/sas.h>
#include <scsi/libsas.h>
#include "isci.h"
#include "sas.h"

 
#define SCIC_SDS_SIGNATURE_FIS_TIMEOUT    25000

 
#define SCIC_SDS_SATA_LINK_TRAINING_TIMEOUT  250

 
struct isci_phy {
	struct sci_base_state_machine sm;
	struct isci_port *owning_port;
	enum sas_linkrate max_negotiated_speed;
	enum sas_protocol protocol;
	u8 phy_index;
	bool bcn_received_while_port_unassigned;
	bool is_in_link_training;
	struct sci_timer sata_timer;
	struct scu_transport_layer_registers __iomem *transport_layer_registers;
	struct scu_link_layer_registers __iomem *link_layer_registers;
	struct asd_sas_phy sas_phy;
	u8 sas_addr[SAS_ADDR_SIZE];
	union {
		struct sas_identify_frame iaf;
		struct dev_to_host_fis fis;
	} frame_rcvd;
};

static inline struct isci_phy *to_iphy(struct asd_sas_phy *sas_phy)
{
	struct isci_phy *iphy = container_of(sas_phy, typeof(*iphy), sas_phy);

	return iphy;
}

struct sci_phy_cap {
	union {
		struct {
			 
			u8 start:1;
			u8 tx_ssc_type:1;
			u8 res1:2;
			u8 req_logical_linkrate:4;

			u32 gen1_no_ssc:1;
			u32 gen1_ssc:1;
			u32 gen2_no_ssc:1;
			u32 gen2_ssc:1;
			u32 gen3_no_ssc:1;
			u32 gen3_ssc:1;
			u32 res2:17;
			u32 parity:1;
		};
		u32 all;
	};
}  __packed;

 
struct sci_phy_proto {
	union {
		struct {
			u16 _r_a:1;
			u16 smp_iport:1;
			u16 stp_iport:1;
			u16 ssp_iport:1;
			u16 _r_b:4;
			u16 _r_c:1;
			u16 smp_tport:1;
			u16 stp_tport:1;
			u16 ssp_tport:1;
			u16 _r_d:4;
		};
		u16 all;
	};
} __packed;


 
struct sci_phy_properties {
	 
	struct isci_port *iport;

	 
	enum sas_linkrate negotiated_link_rate;

	 
	u8 index;
};

 
struct sci_sas_phy_properties {
	 
	struct sas_identify_frame rcvd_iaf;

	 
	struct sci_phy_cap rcvd_cap;

};

 
struct sci_sata_phy_properties {
	 
	struct dev_to_host_fis signature_fis;

	 
	bool is_port_selector_present;

};

 
enum sci_phy_counter_id {
	 
	SCIC_PHY_COUNTER_RECEIVED_FRAME,

	 
	SCIC_PHY_COUNTER_TRANSMITTED_FRAME,

	 
	SCIC_PHY_COUNTER_RECEIVED_FRAME_WORD,

	 
	SCIC_PHY_COUNTER_TRANSMITTED_FRAME_DWORD,

	 
	SCIC_PHY_COUNTER_LOSS_OF_SYNC_ERROR,

	 
	SCIC_PHY_COUNTER_RECEIVED_DISPARITY_ERROR,

	 
	SCIC_PHY_COUNTER_RECEIVED_FRAME_CRC_ERROR,

	 
	SCIC_PHY_COUNTER_RECEIVED_DONE_ACK_NAK_TIMEOUT,

	 
	SCIC_PHY_COUNTER_TRANSMITTED_DONE_ACK_NAK_TIMEOUT,

	 
	SCIC_PHY_COUNTER_INACTIVITY_TIMER_EXPIRED,

	 
	SCIC_PHY_COUNTER_RECEIVED_DONE_CREDIT_TIMEOUT,

	 
	SCIC_PHY_COUNTER_TRANSMITTED_DONE_CREDIT_TIMEOUT,

	 
	SCIC_PHY_COUNTER_RECEIVED_CREDIT_BLOCKED,

	 
	SCIC_PHY_COUNTER_RECEIVED_SHORT_FRAME,

	 
	SCIC_PHY_COUNTER_RECEIVED_FRAME_WITHOUT_CREDIT,

	 
	SCIC_PHY_COUNTER_RECEIVED_FRAME_AFTER_DONE,

	 
	SCIC_PHY_COUNTER_SN_DWORD_SYNC_ERROR
};

 
#define PHY_STATES {\
	C(PHY_INITIAL),\
	C(PHY_STOPPED),\
	C(PHY_STARTING),\
	C(PHY_SUB_INITIAL),\
	C(PHY_SUB_AWAIT_OSSP_EN),\
	C(PHY_SUB_AWAIT_SAS_SPEED_EN),\
	C(PHY_SUB_AWAIT_IAF_UF),\
	C(PHY_SUB_AWAIT_SAS_POWER),\
	C(PHY_SUB_AWAIT_SATA_POWER),\
	C(PHY_SUB_AWAIT_SATA_PHY_EN),\
	C(PHY_SUB_AWAIT_SATA_SPEED_EN),\
	C(PHY_SUB_AWAIT_SIG_FIS_UF),\
	C(PHY_SUB_FINAL),\
	C(PHY_READY),\
	C(PHY_RESETTING),\
	C(PHY_FINAL),\
	}
#undef C
#define C(a) SCI_##a
enum sci_phy_states PHY_STATES;
#undef C

void sci_phy_construct(
	struct isci_phy *iphy,
	struct isci_port *iport,
	u8 phy_index);

struct isci_port *phy_get_non_dummy_port(struct isci_phy *iphy);

void sci_phy_set_port(
	struct isci_phy *iphy,
	struct isci_port *iport);

enum sci_status sci_phy_initialize(
	struct isci_phy *iphy,
	struct scu_transport_layer_registers __iomem *transport_layer_registers,
	struct scu_link_layer_registers __iomem *link_layer_registers);

enum sci_status sci_phy_start(
	struct isci_phy *iphy);

enum sci_status sci_phy_stop(
	struct isci_phy *iphy);

enum sci_status sci_phy_reset(
	struct isci_phy *iphy);

void sci_phy_resume(
	struct isci_phy *iphy);

void sci_phy_setup_transport(
	struct isci_phy *iphy,
	u32 device_id);

enum sci_status sci_phy_event_handler(
	struct isci_phy *iphy,
	u32 event_code);

enum sci_status sci_phy_frame_handler(
	struct isci_phy *iphy,
	u32 frame_index);

enum sci_status sci_phy_consume_power_handler(
	struct isci_phy *iphy);

void sci_phy_get_sas_address(
	struct isci_phy *iphy,
	struct sci_sas_address *sas_address);

void sci_phy_get_attached_sas_address(
	struct isci_phy *iphy,
	struct sci_sas_address *sas_address);

void sci_phy_get_protocols(
	struct isci_phy *iphy,
	struct sci_phy_proto *protocols);
enum sas_linkrate sci_phy_linkrate(struct isci_phy *iphy);

struct isci_host;
void isci_phy_init(struct isci_phy *iphy, struct isci_host *ihost, int index);
int isci_phy_control(struct asd_sas_phy *phy, enum phy_func func, void *buf);

#endif  
