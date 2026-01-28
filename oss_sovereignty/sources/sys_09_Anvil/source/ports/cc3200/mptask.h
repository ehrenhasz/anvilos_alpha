
#ifndef MICROPY_INCLUDED_CC3200_MPTASK_H
#define MICROPY_INCLUDED_CC3200_MPTASK_H


#define MICROPY_TASK_PRIORITY                   (2)
#define MICROPY_TASK_STACK_SIZE                 ((6 * 1024) + 512) 
#define MICROPY_TASK_STACK_LEN                  (MICROPY_TASK_STACK_SIZE / sizeof(StackType_t))


extern StackType_t mpTaskStack[];


extern void TASK_MicroPython(void *pvParameters);

#endif 
