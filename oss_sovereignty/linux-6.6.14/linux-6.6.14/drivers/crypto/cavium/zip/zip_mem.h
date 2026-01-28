#ifndef __ZIP_MEM_H__
#define __ZIP_MEM_H__
void zip_cmd_qbuf_free(struct zip_device *zip, int q);
int zip_cmd_qbuf_alloc(struct zip_device *zip, int q);
u8 *zip_data_buf_alloc(u64 size);
void zip_data_buf_free(u8 *ptr, u64 size);
#endif
