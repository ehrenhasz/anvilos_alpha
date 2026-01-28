int em28xx_start_analog_streaming(struct vb2_queue *vq, unsigned int count);
void em28xx_stop_vbi_streaming(struct vb2_queue *vq);
extern const struct vb2_ops em28xx_vbi_qops;
