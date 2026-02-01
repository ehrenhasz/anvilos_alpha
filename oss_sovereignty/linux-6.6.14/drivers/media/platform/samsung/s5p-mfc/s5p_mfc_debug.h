 
 

#ifndef S5P_MFC_DEBUG_H_
#define S5P_MFC_DEBUG_H_

#define DEBUG

#ifdef DEBUG
extern int mfc_debug_level;

#define mfc_debug(level, fmt, args...)				\
	do {							\
		if (mfc_debug_level >= level)			\
			printk(KERN_DEBUG "%s:%d: " fmt,	\
				__func__, __LINE__, ##args);	\
	} while (0)
#else
#define mfc_debug(level, fmt, args...)
#endif

#define mfc_debug_enter() mfc_debug(5, "enter\n")
#define mfc_debug_leave() mfc_debug(5, "leave\n")

#define mfc_err(fmt, args...)				\
	do {						\
		printk(KERN_ERR "%s:%d: " fmt,		\
		       __func__, __LINE__, ##args);	\
	} while (0)

#define mfc_err_limited(fmt, args...)			\
	do {						\
		printk_ratelimited(KERN_ERR "%s:%d: " fmt,	\
		       __func__, __LINE__, ##args);	\
	} while (0)

#define mfc_info(fmt, args...)				\
	do {						\
		printk(KERN_INFO "%s:%d: " fmt,		\
		       __func__, __LINE__, ##args);	\
	} while (0)

#endif  
