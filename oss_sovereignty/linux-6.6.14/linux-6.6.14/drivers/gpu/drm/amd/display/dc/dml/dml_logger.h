#ifndef __DML_LOGGER_H_
#define __DML_LOGGER_H_
#define DC_LOGGER \
	mode_lib->logger
#define dml_print(str, ...) {DC_LOG_DML(str, ##__VA_ARGS__); }
#define DTRACE(str, ...) {DC_LOG_DML(str, ##__VA_ARGS__); }
#endif
