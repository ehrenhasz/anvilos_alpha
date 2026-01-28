#ifndef _XTENSA_SWITCH_TO_H
#define _XTENSA_SWITCH_TO_H
extern void *_switch_to(void *last, void *next);
#define switch_to(prev,next,last)		\
do {						\
	(last) = _switch_to(prev, next);	\
} while(0)
#endif  
