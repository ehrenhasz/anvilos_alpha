 
 

#ifndef __RCAR_MIPI_DSI_H__
#define __RCAR_MIPI_DSI_H__

struct drm_atomic_state;
struct drm_bridge;

#if IS_ENABLED(CONFIG_DRM_RCAR_MIPI_DSI)
void rcar_mipi_dsi_pclk_enable(struct drm_bridge *bridge,
			       struct drm_atomic_state *state);
void rcar_mipi_dsi_pclk_disable(struct drm_bridge *bridge);
#else
static inline void rcar_mipi_dsi_pclk_enable(struct drm_bridge *bridge,
					     struct drm_atomic_state *state)
{
}

static inline void rcar_mipi_dsi_pclk_disable(struct drm_bridge *bridge)
{
}
#endif  

#endif  
