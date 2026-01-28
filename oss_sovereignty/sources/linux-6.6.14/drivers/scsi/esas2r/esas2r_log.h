

#ifndef __esas2r_log_h__
#define __esas2r_log_h__

struct device;

enum {
	ESAS2R_LOG_NONE = 0,    
	ESAS2R_LOG_CRIT = 1,    
	ESAS2R_LOG_WARN = 2,    
	ESAS2R_LOG_INFO = 3,    
	ESAS2R_LOG_DEBG = 4,    
	ESAS2R_LOG_TRCE = 5,    

#ifdef ESAS2R_TRACE
	ESAS2R_LOG_DFLT = ESAS2R_LOG_TRCE
#else
	ESAS2R_LOG_DFLT = ESAS2R_LOG_WARN
#endif
};

__printf(2, 3) int esas2r_log(const long level, const char *format, ...);
__printf(3, 4) int esas2r_log_dev(const long level,
		   const struct device *dev,
		   const char *format,
		   ...);
int esas2r_log_hexdump(const long level,
		       const void *buf,
		       size_t len);



#ifdef ESAS2R_DEBUG
#define esas2r_debug(f, args ...) esas2r_log(ESAS2R_LOG_DEBG, f, ## args)
#define esas2r_hdebug(f, args ...) esas2r_log(ESAS2R_LOG_DEBG, f, ## args)
#else
#define esas2r_debug(f, args ...)
#define esas2r_hdebug(f, args ...)
#endif  



#ifdef ESAS2R_TRACE
#define esas2r_bugon() \
	do { \
		esas2r_log(ESAS2R_LOG_TRCE, "esas2r_bugon() called in %s:%d" \
			   " - dumping stack and stopping kernel", __func__, \
			   __LINE__); \
		dump_stack(); \
		BUG(); \
	} while (0)

#define esas2r_trace_enter() esas2r_log(ESAS2R_LOG_TRCE, "entered %s (%s:%d)", \
					__func__, __FILE__, __LINE__)
#define esas2r_trace_exit() esas2r_log(ESAS2R_LOG_TRCE, "exited %s (%s:%d)", \
				       __func__, __FILE__, __LINE__)
#define esas2r_trace(f, args ...) esas2r_log(ESAS2R_LOG_TRCE, "(%s:%s:%d): " \
					     f, __func__, __FILE__, __LINE__, \
					     ## args)
#else
#define esas2r_bugon()
#define esas2r_trace_enter()
#define esas2r_trace_exit()
#define esas2r_trace(f, args ...)
#endif  

#endif  
