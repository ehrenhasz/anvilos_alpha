 
 
#ifndef VMW_SO_H
#define VMW_SO_H

enum vmw_view_type {
	vmw_view_sr,
	vmw_view_rt,
	vmw_view_ds,
	vmw_view_ua,
	vmw_view_max,
};

enum vmw_so_type {
	vmw_so_el,
	vmw_so_bs,
	vmw_so_ds,
	vmw_so_rs,
	vmw_so_ss,
	vmw_so_so,
	vmw_so_max,
};

 
union vmw_view_destroy {
	struct SVGA3dCmdDXDestroyRenderTargetView rtv;
	struct SVGA3dCmdDXDestroyShaderResourceView srv;
	struct SVGA3dCmdDXDestroyDepthStencilView dsv;
	struct SVGA3dCmdDXDestroyUAView uav;
	u32 view_id;
};

 
extern const u32 vmw_view_destroy_cmds[];

 
extern const SVGACOTableType vmw_view_cotables[];

 
extern const SVGACOTableType vmw_so_cotables[];

 
static inline enum vmw_view_type vmw_view_cmd_to_type(u32 id)
{
	u32 tmp = (id - SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW) / 2;

	if (id == SVGA_3D_CMD_DX_DEFINE_UA_VIEW ||
	    id == SVGA_3D_CMD_DX_DESTROY_UA_VIEW)
		return vmw_view_ua;

	if (id == SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2)
		return vmw_view_ds;

	if (tmp > (u32)vmw_view_ds)
		return vmw_view_max;

	return (enum vmw_view_type) tmp;
}

 
static inline enum vmw_so_type vmw_so_cmd_to_type(u32 id)
{
	switch (id) {
	case SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT:
	case SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT:
		return vmw_so_el;
	case SVGA_3D_CMD_DX_DEFINE_BLEND_STATE:
	case SVGA_3D_CMD_DX_DESTROY_BLEND_STATE:
		return vmw_so_bs;
	case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE:
	case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE:
		return vmw_so_ds;
	case SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE:
	case SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE_V2:
	case SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE:
		return vmw_so_rs;
	case SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE:
	case SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE:
		return vmw_so_ss;
	case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT:
	case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB:
	case SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT:
		return vmw_so_so;
	default:
		break;
	}
	return vmw_so_max;
}

 
extern int vmw_view_add(struct vmw_cmdbuf_res_manager *man,
			struct vmw_resource *ctx,
			struct vmw_resource *srf,
			enum vmw_view_type view_type,
			u32 user_key,
			const void *cmd,
			size_t cmd_size,
			struct list_head *list);

extern int vmw_view_remove(struct vmw_cmdbuf_res_manager *man,
			   u32 user_key, enum vmw_view_type view_type,
			   struct list_head *list,
			   struct vmw_resource **res_p);

extern void vmw_view_surface_list_destroy(struct vmw_private *dev_priv,
					  struct list_head *view_list);
extern void vmw_view_cotable_list_destroy(struct vmw_private *dev_priv,
					  struct list_head *list,
					  bool readback);
extern struct vmw_resource *vmw_view_srf(struct vmw_resource *res);
extern struct vmw_resource *vmw_view_lookup(struct vmw_cmdbuf_res_manager *man,
					    enum vmw_view_type view_type,
					    u32 user_key);
extern u32 vmw_view_dirtying(struct vmw_resource *res);
#endif
