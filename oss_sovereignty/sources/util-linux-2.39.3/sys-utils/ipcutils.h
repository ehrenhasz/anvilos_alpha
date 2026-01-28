#ifndef UTIL_LINUX_IPCUTILS_H
#define UTIL_LINUX_IPCUTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>


#ifndef SHM_DEST
  
# define SHM_DEST	01000	
# define SHM_LOCKED	02000	
#endif


#ifndef MSG_STAT
# define MSG_STAT	11
# define MSG_INFO	12
#endif

#ifndef SHM_STAT
# define SHM_STAT	13
# define SHM_INFO	14
struct shm_info {
	int used_ids;
	unsigned long shm_tot;		
	unsigned long shm_rss;		
	unsigned long shm_swp;		
	unsigned long swap_attempts;
	unsigned long swap_successes;
};
#endif

#ifndef SEM_STAT
# define SEM_STAT	18
# define SEM_INFO	19
#endif


#ifndef IPC_INFO
# define IPC_INFO	3
#endif


#ifndef HAVE_UNION_SEMUN

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif


#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif


enum {
	IPC_UNIT_DEFAULT,
	IPC_UNIT_BYTES,
	IPC_UNIT_KB,
	IPC_UNIT_HUMAN
};

struct ipc_limits {
	uint64_t	shmmni;		
	uint64_t	shmmax;		
	uint64_t	shmall;		
	uint64_t	shmmin;		

	int		semmni;		
	int		semmsl;		
	int		semmns;		
	int		semopm;		
	unsigned int	semvmx;		

	int		msgmni;		
	uint64_t	msgmax;		
	int		msgmnb;		
};

extern int ipc_msg_get_limits(struct ipc_limits *lim);
extern int ipc_sem_get_limits(struct ipc_limits *lim);
extern int ipc_shm_get_limits(struct ipc_limits *lim);

struct ipc_stat {
	int		id;
	key_t		key;
	uid_t		uid;    
	gid_t		gid;    
	uid_t		cuid;    
	gid_t		cgid;    
	unsigned int	mode;
};

extern void ipc_print_perms(FILE *f, struct ipc_stat *is);
extern void ipc_print_size(int unit, char *msg, uint64_t size, const char *end, int width);


struct shm_data {
	struct ipc_stat	shm_perm;

	uint64_t	shm_nattch;
	uint64_t	shm_segsz;
	int64_t		shm_atim;	
	int64_t		shm_dtim;
	int64_t		shm_ctim;
	pid_t		shm_cprid;
	pid_t		shm_lprid;
	uint64_t	shm_rss;
	uint64_t	shm_swp;

	struct shm_data  *next;
};

extern int ipc_shm_get_info(int id, struct shm_data **shmds);
extern void ipc_shm_free_info(struct shm_data *shmds);


struct sem_elem {
	int	semval;
	int	ncount;		
	int	zcount;		
	pid_t	pid;		
};
struct sem_data {
	struct ipc_stat sem_perm;

	int64_t		sem_ctime;
	int64_t		sem_otime;
	uint64_t	sem_nsems;

	struct sem_elem	*elements;
	struct sem_data *next;
};

extern int ipc_sem_get_info(int id, struct sem_data **semds);
extern void ipc_sem_free_info(struct sem_data *semds);


struct msg_data {
	struct ipc_stat msg_perm;

	int64_t		q_stime;
	int64_t		q_rtime;
	int64_t		q_ctime;
	uint64_t	q_cbytes;
	uint64_t	q_qnum;
	uint64_t	q_qbytes;
	pid_t		q_lspid;
	pid_t		q_lrpid;

	struct msg_data *next;
};

extern int ipc_msg_get_info(int id, struct msg_data **msgds);
extern void ipc_msg_free_info(struct msg_data *msgds);

#endif 
