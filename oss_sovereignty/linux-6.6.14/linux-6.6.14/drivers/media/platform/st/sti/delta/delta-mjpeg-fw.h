#ifndef DELTA_MJPEG_FW_H
#define DELTA_MJPEG_FW_H
struct jpeg_decoded_buffer_address_t {
	u32 luma_p;
	u32 chroma_p;
};
struct jpeg_display_buffer_address_t {
	u32 struct_size;
	u32 display_luma_p;
	u32 display_chroma_p;
	u32 display_decimated_luma_p;
	u32 display_decimated_chroma_p;
};
enum jpeg_rcn_ref_disp_enable_t {
	JPEG_DISP_AUX_EN = 0x00000010,
	JPEG_DISP_MAIN_EN = 0x00000020,
	JPEG_DISP_AUX_MAIN_EN = 0x00000030,
	JPEG_REF_MAIN_EN = 0x00000100,
	JPEG_REF_MAIN_DISP_AUX_EN = 0x00000110,
	JPEG_REF_MAIN_DISP_MAIN_EN = 0x00000120,
	JPEG_REF_MAIN_DISP_MAIN_AUX_EN = 0x00000130
};
enum jpeg_horizontal_deci_factor_t {
	JPEG_HDEC_1 = 0x00000000,
	JPEG_HDEC_ADVANCED_2 = 0x00000101,
	JPEG_HDEC_ADVANCED_4 = 0x00000102
};
enum jpeg_vertical_deci_factor_t {
	JPEG_VDEC_1 = 0x00000000,
	JPEG_VDEC_ADVANCED_2_PROG = 0x00000204,
	JPEG_VDEC_ADVANCED_2_INT = 0x000000208
};
enum jpeg_decoding_error_t {
	JPEG_DECODER_NO_ERROR = 0,
	JPEG_DECODER_UNDEFINED_HUFF_TABLE = 1,
	JPEG_DECODER_UNSUPPORTED_MARKER = 2,
	JPEG_DECODER_UNABLE_ALLOCATE_MEMORY = 3,
	JPEG_DECODER_NON_SUPPORTED_SAMP_FACTORS = 4,
	JPEG_DECODER_BAD_PARAMETER = 5,
	JPEG_DECODER_DECODE_ERROR = 6,
	JPEG_DECODER_BAD_RESTART_MARKER = 7,
	JPEG_DECODER_UNSUPPORTED_COLORSPACE = 8,
	JPEG_DECODER_BAD_SOS_SPECTRAL = 9,
	JPEG_DECODER_BAD_SOS_SUCCESSIVE = 10,
	JPEG_DECODER_BAD_HEADER_LENGTH = 11,
	JPEG_DECODER_BAD_COUNT_VALUE = 12,
	JPEG_DECODER_BAD_DHT_MARKER = 13,
	JPEG_DECODER_BAD_INDEX_VALUE = 14,
	JPEG_DECODER_BAD_NUMBER_HUFFMAN_TABLES = 15,
	JPEG_DECODER_BAD_QUANT_TABLE_LENGTH = 16,
	JPEG_DECODER_BAD_NUMBER_QUANT_TABLES = 17,
	JPEG_DECODER_BAD_COMPONENT_COUNT = 18,
	JPEG_DECODER_DIVIDE_BY_ZERO_ERROR = 19,
	JPEG_DECODER_NOT_JPG_IMAGE = 20,
	JPEG_DECODER_UNSUPPORTED_ROTATION_ANGLE = 21,
	JPEG_DECODER_UNSUPPORTED_SCALING = 22,
	JPEG_DECODER_INSUFFICIENT_OUTPUTBUFFER_SIZE = 23,
	JPEG_DECODER_BAD_HWCFG_GP_VERSION_VALUE = 24,
	JPEG_DECODER_BAD_VALUE_FROM_RED = 25,
	JPEG_DECODER_BAD_SUBREGION_PARAMETERS = 26,
	JPEG_DECODER_PROGRESSIVE_DECODE_NOT_SUPPORTED = 27,
	JPEG_DECODER_ERROR_TASK_TIMEOUT = 28,
	JPEG_DECODER_ERROR_FEATURE_NOT_SUPPORTED = 29
};
enum jpeg_decoding_mode_t {
	JPEG_NORMAL_DECODE = 0,
};
enum jpeg_additional_flags_t {
	JPEG_ADDITIONAL_FLAG_NONE = 0,
	JPEG_ADDITIONAL_FLAG_CEH = 1,
	JPEG_ADDITIONAL_FLAG_RASTER = 64,
	JPEG_ADDITIONAL_FLAG_420MB = 128
};
struct jpeg_video_decode_init_params_t {
	u32 circular_buffer_begin_addr_p;
	u32 circular_buffer_end_addr_p;
	u32 reserved;
};
struct jpeg_decode_params_t {
	u32 picture_start_addr_p;
	u32 picture_end_addr_p;
	struct jpeg_decoded_buffer_address_t decoded_buffer_addr;
	struct jpeg_display_buffer_address_t display_buffer_addr;
	enum jpeg_rcn_ref_disp_enable_t main_aux_enable;
	enum jpeg_horizontal_deci_factor_t horizontal_decimation_factor;
	enum jpeg_vertical_deci_factor_t vertical_decimation_factor;
	u32 xvalue0;
	u32 xvalue1;
	u32 yvalue0;
	u32 yvalue1;
	enum jpeg_decoding_mode_t decoding_mode;
	u32 additional_flags;
	u32 field_flag;
	u32 reserved;
	u32 is_jpeg_image;
};
struct jpeg_decode_return_params_t {
	u32 decode_time_in_us;
	u32 pm_cycles;
	u32 pm_dmiss;
	u32 pm_imiss;
	u32 pm_bundles;
	u32 pm_pft;
	enum jpeg_decoding_error_t error_code;
	u32 ceh_registers[32];
};
#endif  
