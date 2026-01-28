
#ifndef _INPUT_CORE_PRIVATE_H
#define _INPUT_CORE_PRIVATE_H



struct input_dev;

void input_mt_release_slots(struct input_dev *dev);
void input_handle_event(struct input_dev *dev,
			unsigned int type, unsigned int code, int value);

#endif 
