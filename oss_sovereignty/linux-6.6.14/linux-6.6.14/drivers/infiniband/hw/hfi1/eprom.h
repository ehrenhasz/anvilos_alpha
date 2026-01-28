struct hfi1_devdata;
int eprom_init(struct hfi1_devdata *dd);
int eprom_read_platform_config(struct hfi1_devdata *dd, void **buf_ret,
			       u32 *size_ret);
