#ifndef __MLX5_MAPPING_H__
#define __MLX5_MAPPING_H__
struct mapping_ctx;
int mapping_add(struct mapping_ctx *ctx, void *data, u32 *id);
int mapping_remove(struct mapping_ctx *ctx, u32 id);
int mapping_find(struct mapping_ctx *ctx, u32 id, void *data);
struct mapping_ctx *mapping_create(size_t data_size, u32 max_id,
				   bool delayed_removal);
void mapping_destroy(struct mapping_ctx *ctx);
struct mapping_ctx *
mapping_create_for_id(u64 id, u8 type, size_t data_size, u32 max_id, bool delayed_removal);
#endif  
