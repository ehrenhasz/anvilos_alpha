#ifndef CXD2880_COMMON_H
#define CXD2880_COMMON_H
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/bits.h>
#include <linux/string.h>
int cxd2880_convert2s_complement(u32 value, u32 bitlen);
#endif
