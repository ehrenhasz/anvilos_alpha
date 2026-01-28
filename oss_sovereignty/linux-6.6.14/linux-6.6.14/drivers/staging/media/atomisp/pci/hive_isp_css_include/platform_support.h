#ifndef __PLATFORM_SUPPORT_H_INCLUDED__
#define __PLATFORM_SUPPORT_H_INCLUDED__
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/string.h>
#define UINT16_MAX USHRT_MAX
#define UINT32_MAX UINT_MAX
#define UCHAR_MAX  (255)
#define CSS_ALIGN(d, a) d __attribute__((aligned(a)))
#endif  
