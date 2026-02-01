 

#ifndef DM_CP_PSP_IF__H
#define DM_CP_PSP_IF__H

struct dc_link;

struct cp_psp_stream_config {
	uint8_t otg_inst;
	uint8_t dig_be;
	uint8_t dig_fe;
	uint8_t link_enc_idx;
	uint8_t stream_enc_idx;
	uint8_t dio_output_idx;
	uint8_t phy_idx;
	uint8_t assr_enabled;
	uint8_t mst_enabled;
	uint8_t dp2_enabled;
	uint8_t usb4_enabled;
	void *dm_stream_ctx;
	bool dpms_off;
};

struct cp_psp_funcs {
	bool (*enable_assr)(void *handle, struct dc_link *link);
	void (*update_stream_config)(void *handle, struct cp_psp_stream_config *config);
};

struct cp_psp {
	void *handle;
	struct cp_psp_funcs funcs;
};


#endif  
