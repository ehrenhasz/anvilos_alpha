#ifndef _VIVID_KTHREAD_CAP_H_
#define _VIVID_KTHREAD_CAP_H_
int vivid_start_generating_vid_cap(struct vivid_dev *dev, bool *pstreaming);
void vivid_stop_generating_vid_cap(struct vivid_dev *dev, bool *pstreaming);
#endif
