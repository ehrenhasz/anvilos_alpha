 
#ifndef __NOUVEAU_DMEM_H__
#define __NOUVEAU_DMEM_H__
#include <nvif/os.h>
struct drm_device;
struct drm_file;
struct nouveau_drm;
struct nouveau_svmm;
struct hmm_range;

#if IS_ENABLED(CONFIG_DRM_NOUVEAU_SVM)
void nouveau_dmem_init(struct nouveau_drm *);
void nouveau_dmem_fini(struct nouveau_drm *);
void nouveau_dmem_suspend(struct nouveau_drm *);
void nouveau_dmem_resume(struct nouveau_drm *);

int nouveau_dmem_migrate_vma(struct nouveau_drm *drm,
			     struct nouveau_svmm *svmm,
			     struct vm_area_struct *vma,
			     unsigned long start,
			     unsigned long end);
unsigned long nouveau_dmem_page_addr(struct page *page);

#else  
static inline void nouveau_dmem_init(struct nouveau_drm *drm) {}
static inline void nouveau_dmem_fini(struct nouveau_drm *drm) {}
static inline void nouveau_dmem_suspend(struct nouveau_drm *drm) {}
static inline void nouveau_dmem_resume(struct nouveau_drm *drm) {}
#endif  
#endif
