 

#ifndef __DAL_GRPH_OBJECT_ID_H__
#define __DAL_GRPH_OBJECT_ID_H__

 
enum object_type {
	OBJECT_TYPE_UNKNOWN  = 0,

	 
	OBJECT_TYPE_GPU,
	OBJECT_TYPE_ENCODER,
	OBJECT_TYPE_CONNECTOR,
	OBJECT_TYPE_ROUTER,
	OBJECT_TYPE_GENERIC,

	 
	OBJECT_TYPE_AUDIO,
	OBJECT_TYPE_CONTROLLER,
	OBJECT_TYPE_CLOCK_SOURCE,
	OBJECT_TYPE_ENGINE,

	OBJECT_TYPE_COUNT
};

 
enum object_enum_id {
	ENUM_ID_UNKNOWN = 0,
	ENUM_ID_1,
	ENUM_ID_2,
	ENUM_ID_3,
	ENUM_ID_4,
	ENUM_ID_5,
	ENUM_ID_6,
	ENUM_ID_7,

	ENUM_ID_COUNT
};

 
enum generic_id {
	GENERIC_ID_UNKNOWN = 0,
	GENERIC_ID_MXM_OPM,
	GENERIC_ID_GLSYNC,
	GENERIC_ID_STEREO,

	GENERIC_ID_COUNT
};

 
enum controller_id {
	CONTROLLER_ID_UNDEFINED = 0,
	CONTROLLER_ID_D0,
	CONTROLLER_ID_D1,
	CONTROLLER_ID_D2,
	CONTROLLER_ID_D3,
	CONTROLLER_ID_D4,
	CONTROLLER_ID_D5,
	CONTROLLER_ID_UNDERLAY0,
	CONTROLLER_ID_MAX = CONTROLLER_ID_UNDERLAY0
};

#define IS_UNDERLAY_CONTROLLER(ctrlr_id) (ctrlr_id >= CONTROLLER_ID_UNDERLAY0)

 
enum clock_source_id {
	CLOCK_SOURCE_ID_UNDEFINED = 0,
	CLOCK_SOURCE_ID_PLL0,
	CLOCK_SOURCE_ID_PLL1,
	CLOCK_SOURCE_ID_PLL2,
	CLOCK_SOURCE_ID_EXTERNAL,  
	CLOCK_SOURCE_ID_DCPLL,
	CLOCK_SOURCE_ID_DFS,	 
	CLOCK_SOURCE_ID_VCE,	 
	 
	CLOCK_SOURCE_ID_DP_DTO,

	CLOCK_SOURCE_COMBO_PHY_PLL0,  
	CLOCK_SOURCE_COMBO_PHY_PLL1,
	CLOCK_SOURCE_COMBO_PHY_PLL2,
	CLOCK_SOURCE_COMBO_PHY_PLL3,
	CLOCK_SOURCE_COMBO_PHY_PLL4,
	CLOCK_SOURCE_COMBO_PHY_PLL5,
	CLOCK_SOURCE_COMBO_DISPLAY_PLL0
};

 
enum encoder_id {
	ENCODER_ID_UNKNOWN = 0,

	 
	ENCODER_ID_INTERNAL_LVDS,
	ENCODER_ID_INTERNAL_TMDS1,
	ENCODER_ID_INTERNAL_TMDS2,
	ENCODER_ID_INTERNAL_DAC1,
	ENCODER_ID_INTERNAL_DAC2,	 

	 
	ENCODER_ID_INTERNAL_LVTM1,	 
	ENCODER_ID_INTERNAL_HDMI,

	 
	ENCODER_ID_INTERNAL_KLDSCP_TMDS1,
	ENCODER_ID_INTERNAL_KLDSCP_DAC1,
	ENCODER_ID_INTERNAL_KLDSCP_DAC2,	 
	 
	ENCODER_ID_EXTERNAL_MVPU_FPGA,	 
	ENCODER_ID_INTERNAL_DDI,
	ENCODER_ID_INTERNAL_UNIPHY,
	ENCODER_ID_INTERNAL_KLDSCP_LVTMA,
	ENCODER_ID_INTERNAL_UNIPHY1,
	ENCODER_ID_INTERNAL_UNIPHY2,
	ENCODER_ID_EXTERNAL_NUTMEG,
	ENCODER_ID_EXTERNAL_TRAVIS,

	ENCODER_ID_INTERNAL_WIRELESS,	 
	ENCODER_ID_INTERNAL_UNIPHY3,
	ENCODER_ID_INTERNAL_VIRTUAL,
};

 
enum connector_id {
	CONNECTOR_ID_UNKNOWN = 0,
	CONNECTOR_ID_SINGLE_LINK_DVII = 1,
	CONNECTOR_ID_DUAL_LINK_DVII = 2,
	CONNECTOR_ID_SINGLE_LINK_DVID = 3,
	CONNECTOR_ID_DUAL_LINK_DVID = 4,
	CONNECTOR_ID_VGA = 5,
	CONNECTOR_ID_HDMI_TYPE_A = 12,
	CONNECTOR_ID_LVDS = 14,
	CONNECTOR_ID_PCIE = 16,
	CONNECTOR_ID_HARDCODE_DVI = 18,
	CONNECTOR_ID_DISPLAY_PORT = 19,
	CONNECTOR_ID_EDP = 20,
	CONNECTOR_ID_MXM = 21,
	CONNECTOR_ID_WIRELESS = 22,
	CONNECTOR_ID_MIRACAST = 23,
	CONNECTOR_ID_USBC = 24,

	CONNECTOR_ID_VIRTUAL = 100
};

 
enum audio_id {
	AUDIO_ID_UNKNOWN = 0,
	AUDIO_ID_INTERNAL_AZALIA
};

 
enum engine_id {
	ENGINE_ID_DIGA,
	ENGINE_ID_DIGB,
	ENGINE_ID_DIGC,
	ENGINE_ID_DIGD,
	ENGINE_ID_DIGE,
	ENGINE_ID_DIGF,
	ENGINE_ID_DIGG,
	ENGINE_ID_DACA,
	ENGINE_ID_DACB,
	ENGINE_ID_VCE,	 
	ENGINE_ID_HPO_0,
	ENGINE_ID_HPO_1,
	ENGINE_ID_HPO_DP_0,
	ENGINE_ID_HPO_DP_1,
	ENGINE_ID_HPO_DP_2,
	ENGINE_ID_HPO_DP_3,
	ENGINE_ID_VIRTUAL,

	ENGINE_ID_COUNT,
	ENGINE_ID_UNKNOWN = (-1L)
};

enum transmitter_color_depth {
	TRANSMITTER_COLOR_DEPTH_24 = 0,   
	TRANSMITTER_COLOR_DEPTH_30,       
	TRANSMITTER_COLOR_DEPTH_36,       
	TRANSMITTER_COLOR_DEPTH_48        
};

enum dp_alt_mode {
	DP_Alt_mode__Unknown = 0,
	DP_Alt_mode__Connect,
	DP_Alt_mode__NoConnect,
};
 

struct graphics_object_id {
	uint32_t  id:8;
	uint32_t  enum_id:4;
	uint32_t  type:4;
	uint32_t  reserved:16;  
};

 

static inline struct graphics_object_id dal_graphics_object_id_init(
	uint32_t id,
	enum object_enum_id enum_id,
	enum object_type type)
{
	struct graphics_object_id result = {
		id, enum_id, type, 0
	};

	return result;
}

 
static inline uint32_t dal_graphics_object_id_to_uint(
	struct graphics_object_id id)
{
	return id.id + (id.enum_id << 0x8) + (id.type << 0xc);
}

static inline enum controller_id dal_graphics_object_id_get_controller_id(
	struct graphics_object_id id)
{
	if (id.type == OBJECT_TYPE_CONTROLLER)
		return (enum controller_id) id.id;
	return CONTROLLER_ID_UNDEFINED;
}

static inline enum clock_source_id dal_graphics_object_id_get_clock_source_id(
	struct graphics_object_id id)
{
	if (id.type == OBJECT_TYPE_CLOCK_SOURCE)
		return (enum clock_source_id) id.id;
	return CLOCK_SOURCE_ID_UNDEFINED;
}

static inline enum encoder_id dal_graphics_object_id_get_encoder_id(
	struct graphics_object_id id)
{
	if (id.type == OBJECT_TYPE_ENCODER)
		return (enum encoder_id) id.id;
	return ENCODER_ID_UNKNOWN;
}

static inline enum connector_id dal_graphics_object_id_get_connector_id(
	struct graphics_object_id id)
{
	if (id.type == OBJECT_TYPE_CONNECTOR)
		return (enum connector_id) id.id;
	return CONNECTOR_ID_UNKNOWN;
}

static inline enum audio_id dal_graphics_object_id_get_audio_id(
	struct graphics_object_id id)
{
	if (id.type == OBJECT_TYPE_AUDIO)
		return (enum audio_id) id.id;
	return AUDIO_ID_UNKNOWN;
}

static inline enum engine_id dal_graphics_object_id_get_engine_id(
	struct graphics_object_id id)
{
	if (id.type == OBJECT_TYPE_ENGINE)
		return (enum engine_id) id.id;
	return ENGINE_ID_UNKNOWN;
}

static inline bool dal_graphics_object_id_equal(
	struct graphics_object_id id_1,
	struct graphics_object_id id_2)
{
	if ((id_1.id == id_2.id) && (id_1.enum_id == id_2.enum_id) &&
		(id_1.type == id_2.type)) {
		return true;
	}
	return false;
}
#endif
