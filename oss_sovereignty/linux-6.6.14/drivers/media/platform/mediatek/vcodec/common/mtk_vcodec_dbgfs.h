 
 

#ifndef __MTK_VCODEC_DBGFS_H__
#define __MTK_VCODEC_DBGFS_H__

struct mtk_vcodec_dec_dev;
struct mtk_vcodec_dec_ctx;

 
enum mtk_vdec_dbgfs_log_index {
	MTK_VDEC_DBGFS_PICINFO,
	MTK_VDEC_DBGFS_FORMAT,
	MTK_VDEC_DBGFS_MAX,
};

 
struct mtk_vcodec_dbgfs_inst {
	struct list_head node;
	struct mtk_vcodec_dec_ctx *vcodec_ctx;
	int inst_id;
};

 
struct mtk_vcodec_dbgfs {
	struct list_head dbgfs_head;
	struct dentry *vcodec_root;
	struct mutex dbgfs_lock;
	char dbgfs_buf[1024];
	int buf_size;
	int inst_count;
};

#if defined(CONFIG_DEBUG_FS)
void mtk_vcodec_dbgfs_create(struct mtk_vcodec_dec_ctx *ctx);
void mtk_vcodec_dbgfs_remove(struct mtk_vcodec_dec_dev *vcodec_dev, int ctx_id);
void mtk_vcodec_dbgfs_init(void *vcodec_dev, bool is_encode);
void mtk_vcodec_dbgfs_deinit(struct mtk_vcodec_dbgfs *dbgfs);
#else
static inline void mtk_vcodec_dbgfs_create(struct mtk_vcodec_dec_ctx *ctx)
{
}

static inline void mtk_vcodec_dbgfs_remove(struct mtk_vcodec_dec_dev *vcodec_dev, int ctx_id)
{
}

static inline void mtk_vcodec_dbgfs_init(void *vcodec_dev, bool is_encode)
{
}

static inline void mtk_vcodec_dbgfs_deinit(struct mtk_vcodec_dbgfs *dbgfs)
{
}
#endif
#endif
