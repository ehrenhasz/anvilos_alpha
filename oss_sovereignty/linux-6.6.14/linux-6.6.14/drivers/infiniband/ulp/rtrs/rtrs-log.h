#ifndef RTRS_LOG_H
#define RTRS_LOG_H
#define rtrs_log(fn, obj, fmt, ...)				\
	fn("<%s>: " fmt, obj->sessname, ##__VA_ARGS__)
#define rtrs_err(obj, fmt, ...)	\
	rtrs_log(pr_err, obj, fmt, ##__VA_ARGS__)
#define rtrs_err_rl(obj, fmt, ...)	\
	rtrs_log(pr_err_ratelimited, obj, fmt, ##__VA_ARGS__)
#define rtrs_wrn(obj, fmt, ...)	\
	rtrs_log(pr_warn, obj, fmt, ##__VA_ARGS__)
#define rtrs_wrn_rl(obj, fmt, ...) \
	rtrs_log(pr_warn_ratelimited, obj, fmt, ##__VA_ARGS__)
#define rtrs_info(obj, fmt, ...) \
	rtrs_log(pr_info, obj, fmt, ##__VA_ARGS__)
#define rtrs_info_rl(obj, fmt, ...) \
	rtrs_log(pr_info_ratelimited, obj, fmt, ##__VA_ARGS__)
#endif  
