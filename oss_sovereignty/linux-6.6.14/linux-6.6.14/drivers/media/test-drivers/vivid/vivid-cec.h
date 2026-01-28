#ifdef CONFIG_VIDEO_VIVID_CEC
struct cec_adapter *vivid_cec_alloc_adap(struct vivid_dev *dev,
					 unsigned int idx,
					 bool is_source);
int vivid_cec_bus_thread(void *_dev);
#endif
