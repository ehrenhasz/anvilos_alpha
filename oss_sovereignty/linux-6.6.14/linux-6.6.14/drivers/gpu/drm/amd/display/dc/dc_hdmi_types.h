#ifndef DC_HDMI_TYPES_H
#define DC_HDMI_TYPES_H
#include "os_types.h"
#define DP_ADAPTOR_TYPE2_SIZE 0x20
#define DP_ADAPTOR_TYPE2_REG_ID 0x10
#define DP_ADAPTOR_TYPE2_REG_MAX_TMDS_CLK 0x1D
#define DP_ADAPTOR_TYPE2_ID 0xA0
#define DP_ADAPTOR_TYPE2_MAX_TMDS_CLK 600
#define DP_ADAPTOR_TYPE2_MIN_TMDS_CLK 25
#define DP_ADAPTOR_DVI_MAX_TMDS_CLK 165000
#define DP_ADAPTOR_HDMI_SAFE_MAX_TMDS_CLK 165000
struct dp_hdmi_dongle_signature_data {
	int8_t id[15]; 
	uint8_t eot; 
};
#define DP_HDMI_DONGLE_ADDRESS 0x40
#define DP_HDMI_DONGLE_SIGNATURE_EOT 0x04
#define HDMI_SCDC_WRITE_UPDATE_0_ARRAY 3
#define HDMI_SCDC_ADDRESS  0x54
#define HDMI_SCDC_SINK_VERSION 0x01
#define HDMI_SCDC_SOURCE_VERSION 0x02
#define HDMI_SCDC_UPDATE_0 0x10
#define HDMI_SCDC_TMDS_CONFIG 0x20
#define HDMI_SCDC_SCRAMBLER_STATUS 0x21
#define HDMI_SCDC_CONFIG_0 0x30
#define HDMI_SCDC_CONFIG_1 0x31
#define HDMI_SCDC_SOURCE_TEST_REQ 0x35
#define HDMI_SCDC_STATUS_FLAGS 0x40
#define HDMI_SCDC_ERR_DETECT 0x50
#define HDMI_SCDC_TEST_CONFIG 0xC0
#define HDMI_SCDC_MANUFACTURER_OUI 0xD0
#define HDMI_SCDC_DEVICE_ID 0xDB
union hdmi_scdc_update_read_data {
	uint8_t byte[2];
	struct {
		uint8_t STATUS_UPDATE:1;
		uint8_t CED_UPDATE:1;
		uint8_t RR_TEST:1;
		uint8_t RESERVED:5;
		uint8_t RESERVED2:8;
	} fields;
};
union hdmi_scdc_status_flags_data {
	uint8_t byte;
	struct {
		uint8_t CLOCK_DETECTED:1;
		uint8_t CH0_LOCKED:1;
		uint8_t CH1_LOCKED:1;
		uint8_t CH2_LOCKED:1;
		uint8_t RESERVED:4;
	} fields;
};
union hdmi_scdc_ced_data {
	uint8_t byte[11];
	struct {
		uint8_t CH0_8LOW:8;
		uint8_t CH0_7HIGH:7;
		uint8_t CH0_VALID:1;
		uint8_t CH1_8LOW:8;
		uint8_t CH1_7HIGH:7;
		uint8_t CH1_VALID:1;
		uint8_t CH2_8LOW:8;
		uint8_t CH2_7HIGH:7;
		uint8_t CH2_VALID:1;
		uint8_t CHECKSUM:8;
		uint8_t RESERVED:8;
		uint8_t RESERVED2:8;
		uint8_t RESERVED3:8;
		uint8_t RESERVED4:4;
	} fields;
};
union hdmi_scdc_manufacturer_OUI_data {
	uint8_t byte[3];
	struct {
		uint8_t Manufacturer_OUI_1:8;
		uint8_t Manufacturer_OUI_2:8;
		uint8_t Manufacturer_OUI_3:8;
	} fields;
};
union hdmi_scdc_device_id_data {
	uint8_t byte;
	struct {
		uint8_t Hardware_Minor_Rev:4;
		uint8_t Hardware_Major_Rev:4;
	} fields;
};
#endif  
