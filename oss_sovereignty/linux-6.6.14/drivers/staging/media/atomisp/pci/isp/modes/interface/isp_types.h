 
 

#ifndef _ISP_TYPES_H_
#define _ISP_TYPES_H_

 
struct ia_css_3a_output;

 
enum sh_stream_format {
	sh_stream_format_yuv420_legacy,
	sh_stream_format_yuv420,
	sh_stream_format_yuv422,
	sh_stream_format_rgb,
	sh_stream_format_raw,
	sh_stream_format_binary,	 
};

struct s_isp_frames {
	 
	 
	char *xmem_base_addr_y;
	char *xmem_base_addr_uv;
	char *xmem_base_addr_u;
	char *xmem_base_addr_v;
	 
	char *xmem_base_addr_second_out_y;
	char *xmem_base_addr_second_out_u;
	char *xmem_base_addr_second_out_v;
	 
	char *xmem_base_addr_y_in;
	char *xmem_base_addr_u_in;
	char *xmem_base_addr_v_in;
	 
	char *xmem_base_addr_raw;
	 
	char *xmem_base_addr_raw_out;
	 
	char *xmem_base_addr_vfout_y;
	char *xmem_base_addr_vfout_u;
	char *xmem_base_addr_vfout_v;
	 
	char *xmem_base_addr_overlay_y;
	char *xmem_base_addr_overlay_u;
	char *xmem_base_addr_overlay_v;
	 
	char *xmem_base_addr_qplane_r;
	char *xmem_base_addr_qplane_ratb;
	char *xmem_base_addr_qplane_gr;
	char *xmem_base_addr_qplane_gb;
	char *xmem_base_addr_qplane_b;
	char *xmem_base_addr_qplane_batr;
	 
	char *xmem_base_addr_yuv_16_y;
	char *xmem_base_addr_yuv_16_u;
	char *xmem_base_addr_yuv_16_v;
};

#endif  
