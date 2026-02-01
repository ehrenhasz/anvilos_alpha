
 
 

#include "vmwgfx_drv.h"
#include "ttm_object.h"
#include <linux/dma-buf.h>

 

static int vmw_prime_map_attach(struct dma_buf *dma_buf,
				struct dma_buf_attachment *attach)
{
	return -ENOSYS;
}

static void vmw_prime_map_detach(struct dma_buf *dma_buf,
				 struct dma_buf_attachment *attach)
{
}

static struct sg_table *vmw_prime_map_dma_buf(struct dma_buf_attachment *attach,
					      enum dma_data_direction dir)
{
	return ERR_PTR(-ENOSYS);
}

static void vmw_prime_unmap_dma_buf(struct dma_buf_attachment *attach,
				    struct sg_table *sgb,
				    enum dma_data_direction dir)
{
}

const struct dma_buf_ops vmw_prime_dmabuf_ops =  {
	.attach = vmw_prime_map_attach,
	.detach = vmw_prime_map_detach,
	.map_dma_buf = vmw_prime_map_dma_buf,
	.unmap_dma_buf = vmw_prime_unmap_dma_buf,
	.release = NULL,
};

int vmw_prime_fd_to_handle(struct drm_device *dev,
			   struct drm_file *file_priv,
			   int fd, u32 *handle)
{
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;

	return ttm_prime_fd_to_handle(tfile, fd, handle);
}

int vmw_prime_handle_to_fd(struct drm_device *dev,
			   struct drm_file *file_priv,
			   uint32_t handle, uint32_t flags,
			   int *prime_fd)
{
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;
	return ttm_prime_handle_to_fd(tfile, handle, flags, prime_fd);
}
