 

#ifndef __COMMON_H__
#define __COMMON_H__

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/types.h>

 
#include "zip_device.h"

 
#include "zip_main.h"

 
#include "zip_mem.h"

 
#include "zip_regs.h"

#define ZIP_ERROR    -1

#define ZIP_FLUSH_FINISH  4

#define RAW_FORMAT		0   
#define ZLIB_FORMAT		1   
#define GZIP_FORMAT		2   
#define LZS_FORMAT		3   

 
#define MAX_ZIP_DEVICES		2

 
#define ZIP_NUM_QUEUES		2

#define DYNAMIC_STOP_EXCESS	1024

 
#define MAX_INPUT_BUFFER_SIZE   (64 * 1024)
#define MAX_OUTPUT_BUFFER_SIZE  (64 * 1024)

 
struct zip_operation {
	u8    *input;
	u8    *output;
	u64   ctx_addr;
	u64   history;

	u32   input_len;
	u32   input_total_len;

	u32   output_len;
	u32   output_total_len;

	u32   csum;
	u32   flush;

	u32   format;
	u32   speed;
	u32   ccode;
	u32   lzs_flag;

	u32   begin_file;
	u32   history_len;

	u32   end_file;
	u32   compcode;
	u32   bytes_read;
	u32   bits_processed;

	u32   sizeofptr;
	u32   sizeofzops;
};

static inline int zip_poll_result(union zip_zres_s *result)
{
	int retries = 1000;

	while (!result->s.compcode) {
		if (!--retries) {
			pr_err("ZIP ERR: request timed out");
			return -ETIMEDOUT;
		}
		udelay(10);
		 
		rmb();
	}
	return 0;
}

 
#define zip_err(fmt, args...) pr_err("ZIP ERR:%s():%d: " \
			      fmt "\n", __func__, __LINE__, ## args)

#ifdef MSG_ENABLE
 
#define zip_msg(fmt, args...) pr_info("ZIP_MSG:" fmt "\n", ## args)
#else
#define zip_msg(fmt, args...)
#endif

#if defined(ZIP_DEBUG_ENABLE) && defined(MSG_ENABLE)

#ifdef DEBUG_LEVEL

#define FILE_NAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
	strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#if DEBUG_LEVEL >= 4

#define zip_dbg(fmt, args...) pr_info("ZIP DBG: %s: %s() : %d: " \
			      fmt "\n", FILE_NAME, __func__, __LINE__, ## args)

#elif DEBUG_LEVEL >= 3

#define zip_dbg(fmt, args...) pr_info("ZIP DBG: %s: %s() : %d: " \
			      fmt "\n", FILE_NAME, __func__, __LINE__, ## args)

#elif DEBUG_LEVEL >= 2

#define zip_dbg(fmt, args...) pr_info("ZIP DBG: %s() : %d: " \
			      fmt "\n", __func__, __LINE__, ## args)

#else

#define zip_dbg(fmt, args...) pr_info("ZIP DBG:" fmt "\n", ## args)

#endif  

#else

#define zip_dbg(fmt, args...) pr_info("ZIP DBG:" fmt "\n", ## args)

#endif  
#else

#define zip_dbg(fmt, args...)

#endif  

#endif
