#ifndef _STI_VID_H_
#define _STI_VID_H_
struct sti_vid {
	struct device *dev;
	void __iomem *regs;
	int id;
};
void sti_vid_commit(struct sti_vid *vid,
		    struct drm_plane_state *state);
void sti_vid_disable(struct sti_vid *vid);
struct sti_vid *sti_vid_create(struct device *dev, struct drm_device *drm_dev,
			       int id, void __iomem *baseaddr);
void vid_debugfs_init(struct sti_vid *vid, struct drm_minor *minor);
#endif
