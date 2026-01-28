

#ifndef _UUID_UUIDD_H
#define _UUID_UUIDD_H

#define UUIDD_DIR		_PATH_RUNSTATEDIR "/uuidd"
#define UUIDD_SOCKET_PATH	UUIDD_DIR "/request"
#define UUIDD_PIDFILE_PATH	UUIDD_DIR "/uuidd.pid"
#define UUIDD_PATH		"/usr/sbin/uuidd"

#define UUIDD_OP_GETPID			0
#define UUIDD_OP_GET_MAXOP		1
#define UUIDD_OP_TIME_UUID		2
#define UUIDD_OP_RANDOM_UUID		3
#define UUIDD_OP_BULK_TIME_UUID		4
#define UUIDD_OP_BULK_RANDOM_UUID	5
#define UUIDD_MAX_OP			UUIDD_OP_BULK_RANDOM_UUID

extern int __uuid_generate_time(uuid_t out, int *num);
extern int __uuid_generate_time_cont(uuid_t out, int *num, uint32_t cont);
extern int __uuid_generate_random(uuid_t out, int *num);

#endif 
