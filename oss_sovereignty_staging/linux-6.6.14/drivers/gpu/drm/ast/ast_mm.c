 
 

#include <linux/pci.h>

#include <drm/drm_managed.h>
#include <drm/drm_print.h>

#include "ast_drv.h"

static u32 ast_get_vram_size(struct ast_device *ast)
{
	u8 jreg;
	u32 vram_size;

	vram_size = AST_VIDMEM_DEFAULT_SIZE;
	jreg = ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0xaa, 0xff);
	switch (jreg & 3) {
	case 0:
		vram_size = AST_VIDMEM_SIZE_8M;
		break;
	case 1:
		vram_size = AST_VIDMEM_SIZE_16M;
		break;
	case 2:
		vram_size = AST_VIDMEM_SIZE_32M;
		break;
	case 3:
		vram_size = AST_VIDMEM_SIZE_64M;
		break;
	}

	jreg = ast_get_index_reg_mask(ast, AST_IO_CRTC_PORT, 0x99, 0xff);
	switch (jreg & 0x03) {
	case 1:
		vram_size -= 0x100000;
		break;
	case 2:
		vram_size -= 0x200000;
		break;
	case 3:
		vram_size -= 0x400000;
		break;
	}

	return vram_size;
}

int ast_mm_init(struct ast_device *ast)
{
	struct drm_device *dev = &ast->base;
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	resource_size_t base, size;
	u32 vram_size;

	base = pci_resource_start(pdev, 0);
	size = pci_resource_len(pdev, 0);

	 
	devm_arch_io_reserve_memtype_wc(dev->dev, base, size);
	devm_arch_phys_wc_add(dev->dev, base, size);

	vram_size = ast_get_vram_size(ast);

	ast->vram = devm_ioremap_wc(dev->dev, base, vram_size);
	if (!ast->vram)
		return -ENOMEM;

	ast->vram_base = base;
	ast->vram_size = vram_size;
	ast->vram_fb_available = vram_size;

	return 0;
}
