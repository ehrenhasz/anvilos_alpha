 
 
#ifndef __SND_SEQ_QUEUE_H
#define __SND_SEQ_QUEUE_H

#include "seq_memory.h"
#include "seq_prioq.h"
#include "seq_timer.h"
#include "seq_lock.h"
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/bitops.h>

#define SEQ_QUEUE_NO_OWNER (-1)

struct snd_seq_queue {
	int queue;		 

	char name[64];		 

	struct snd_seq_prioq	*tickq;		 
	struct snd_seq_prioq	*timeq;		 	
	
	struct snd_seq_timer *timer;	 
	int	owner;		 
	bool	locked;		 
	bool	klocked;	 
	bool	check_again;	 
	bool	check_blocked;	 

	unsigned int flags;		 
	unsigned int info_flags;	 

	spinlock_t owner_lock;
	spinlock_t check_lock;

	 
	DECLARE_BITMAP(clients_bitmap, SNDRV_SEQ_MAX_CLIENTS);
	unsigned int clients;	 
	struct mutex timer_mutex;

	snd_use_lock_t use_lock;
};


 
int snd_seq_queue_get_cur_queues(void);

  
void snd_seq_queues_delete(void);


 
struct snd_seq_queue *snd_seq_queue_alloc(int client, int locked, unsigned int flags);

 
int snd_seq_queue_delete(int client, int queueid);

 
void snd_seq_queue_client_leave(int client);

 
int snd_seq_enqueue_event(struct snd_seq_event_cell *cell, int atomic, int hop);

 
void snd_seq_queue_client_leave_cells(int client);
void snd_seq_queue_remove_cells(int client, struct snd_seq_remove_events *info);

 
struct snd_seq_queue *queueptr(int queueid);
 
#define queuefree(q) snd_use_lock_free(&(q)->use_lock)

 
struct snd_seq_queue *snd_seq_queue_find_name(char *name);

 
void snd_seq_check_queue(struct snd_seq_queue *q, int atomic, int hop);

 
int snd_seq_queue_check_access(int queueid, int client);
int snd_seq_queue_timer_set_tempo(int queueid, int client, struct snd_seq_queue_tempo *info);
int snd_seq_queue_set_owner(int queueid, int client, int locked);
int snd_seq_queue_set_locked(int queueid, int client, int locked);
int snd_seq_queue_timer_open(int queueid);
int snd_seq_queue_timer_close(int queueid);
int snd_seq_queue_use(int queueid, int client, int use);
int snd_seq_queue_is_used(int queueid, int client);

int snd_seq_control_queue(struct snd_seq_event *ev, int atomic, int hop);

#endif
