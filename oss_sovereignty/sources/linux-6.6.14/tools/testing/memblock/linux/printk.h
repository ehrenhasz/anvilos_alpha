
#ifndef _PRINTK_H
#define _PRINTK_H

#include <stdio.h>
#include <asm/bug.h>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#define printk printf
#pragma GCC diagnostic push

#define pr_info printk
#define pr_debug printk
#define pr_cont printk
#define pr_err printk
#define pr_warn printk

#endif
