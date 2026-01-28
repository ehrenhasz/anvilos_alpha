#ifndef MOD_SHARED_H_
#define MOD_SHARED_H_
enum color_transfer_func {
	TRANSFER_FUNC_UNKNOWN,
	TRANSFER_FUNC_SRGB,
	TRANSFER_FUNC_BT709,
	TRANSFER_FUNC_PQ2084,
	TRANSFER_FUNC_PQ2084_INTERIM,
	TRANSFER_FUNC_LINEAR_0_1,
	TRANSFER_FUNC_LINEAR_0_125,
	TRANSFER_FUNC_GAMMA_22,
	TRANSFER_FUNC_GAMMA_26
};
enum vrr_packet_type {
	PACKET_TYPE_VRR,
	PACKET_TYPE_FS_V1,
	PACKET_TYPE_FS_V2,
	PACKET_TYPE_FS_V3,
	PACKET_TYPE_VTEM
};
union lut3d_control_flags {
	unsigned int raw;
	struct {
		unsigned int do_chroma_scale				:1;
		unsigned int spec_version				:3;
		unsigned int use_zero_display_black			:1;
		unsigned int use_zero_source_black			:1;
		unsigned int force_display_black			:6;
		unsigned int apply_display_gamma			:1;
		unsigned int exp_shaper_max				:6;
		unsigned int unity_3dlut				:1;
		unsigned int bypass_3dlut				:1;
		unsigned int use_3dlut					:1;
		unsigned int less_than_dcip3				:1;
		unsigned int override_lum				:1;
		unsigned int use_gamut_map_lib					:1;
		unsigned int chromatic_adaptation_src				:1;
		unsigned int chromatic_adaptation_dst				:1;
		unsigned int do_blender_lut_degamma		:1;
		unsigned int reseved					:4;
	} bits;
};
enum tm_show_option_internal {
	tm_show_option_internal_single_file		= 0, 
	tm_show_option_internal_duplicate_file,		 
	tm_show_option_internal_duplicate_sidebyside 
};
enum lut3d_control_gamut_map {
	lut3d_control_gamut_map_none = 0,
	lut3d_control_gamut_map_tonemap,
	lut3d_control_gamut_map_chto,
	lut3d_control_gamut_map_chso,
	lut3d_control_gamut_map_chci
};
enum lut3d_control_rotation_mode {
	lut3d_control_rotation_mode_none = 0,
	lut3d_control_rotation_mode_hue,
	lut3d_control_rotation_mode_cc,
	lut3d_control_rotation_mode_hue_cc
};
struct lut3d_settings {
	unsigned char version;
	union lut3d_control_flags flags;
	union lut3d_control_flags flags2;
	enum tm_show_option_internal option;
	unsigned int min_lum; 
	unsigned int max_lum;
	unsigned int min_lum2;
	unsigned int max_lum2;
	enum lut3d_control_gamut_map map;
	enum lut3d_control_rotation_mode rotation;
	enum lut3d_control_gamut_map map2;
	enum lut3d_control_rotation_mode rotation2;
};
#endif  
