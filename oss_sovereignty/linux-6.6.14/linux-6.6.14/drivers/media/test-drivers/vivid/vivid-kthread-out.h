#ifndef _VIVID_KTHREAD_OUT_H_
#define _VIVID_KTHREAD_OUT_H_
int vivid_start_generating_vid_out(struct vivid_dev *dev, bool *pstreaming);
void vivid_stop_generating_vid_out(struct vivid_dev *dev, bool *pstreaming);
#endif
