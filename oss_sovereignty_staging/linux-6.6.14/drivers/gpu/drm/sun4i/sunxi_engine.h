 
 

#ifndef _SUNXI_ENGINE_H_
#define _SUNXI_ENGINE_H_

struct drm_plane;
struct drm_device;
struct drm_crtc_state;
struct drm_display_mode;

struct sunxi_engine;

 
struct sunxi_engine_ops {
	 
	void (*atomic_begin)(struct sunxi_engine *engine,
			     struct drm_crtc_state *old_state);

	 
	int (*atomic_check)(struct sunxi_engine *engine,
			    struct drm_crtc_state *state);

	 
	void (*commit)(struct sunxi_engine *engine);

	 
	struct drm_plane **(*layers_init)(struct drm_device *drm,
					  struct sunxi_engine *engine);

	 
	void (*apply_color_correction)(struct sunxi_engine *engine);

	 
	void (*disable_color_correction)(struct sunxi_engine *engine);

	 
	void (*vblank_quirk)(struct sunxi_engine *engine);

	 
	void (*mode_set)(struct sunxi_engine *engine,
			 const struct drm_display_mode *mode);
};

 
struct sunxi_engine {
	const struct sunxi_engine_ops	*ops;

	struct device_node		*node;
	struct regmap			*regs;

	int id;

	 
	struct list_head		list;
};

 
static inline void
sunxi_engine_commit(struct sunxi_engine *engine)
{
	if (engine->ops && engine->ops->commit)
		engine->ops->commit(engine);
}

 
static inline struct drm_plane **
sunxi_engine_layers_init(struct drm_device *drm, struct sunxi_engine *engine)
{
	if (engine->ops && engine->ops->layers_init)
		return engine->ops->layers_init(drm, engine);
	return ERR_PTR(-ENOSYS);
}

 
static inline void
sunxi_engine_apply_color_correction(struct sunxi_engine *engine)
{
	if (engine->ops && engine->ops->apply_color_correction)
		engine->ops->apply_color_correction(engine);
}

 
static inline void
sunxi_engine_disable_color_correction(struct sunxi_engine *engine)
{
	if (engine->ops && engine->ops->disable_color_correction)
		engine->ops->disable_color_correction(engine);
}

 
static inline void
sunxi_engine_mode_set(struct sunxi_engine *engine,
		      const struct drm_display_mode *mode)
{
	if (engine->ops && engine->ops->mode_set)
		engine->ops->mode_set(engine, mode);
}
#endif  
