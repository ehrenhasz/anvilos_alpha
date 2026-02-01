 
 
#ifndef __SND_SEQ_CLIENTMGR_H
#define __SND_SEQ_CLIENTMGR_H

#include <sound/seq_kernel.h>
#include <linux/bitops.h>
#include "seq_fifo.h"
#include "seq_ports.h"
#include "seq_lock.h"

 

struct snd_seq_user_client {
	struct file *file;	 
	 
	struct pid *owner;
	
	 
	struct snd_seq_fifo *fifo;	 
	int fifo_pool_size;
};

struct snd_seq_kernel_client {
	 
	struct snd_card *card;
};


struct snd_seq_client {
	snd_seq_client_type_t type;
	unsigned int accept_input: 1,
		accept_output: 1;
	unsigned int midi_version;
	unsigned int user_pversion;
	char name[64];		 
	int number;		 
	unsigned int filter;	 
	DECLARE_BITMAP(event_filter, 256);
	unsigned short group_filter;
	snd_use_lock_t use_lock;
	int event_lost;
	 
	int num_ports;		 
	struct list_head ports_list_head;
	rwlock_t ports_lock;
	struct mutex ports_mutex;
	struct mutex ioctl_mutex;
	int convert32;		 
	int ump_endpoint_port;

	 
	struct snd_seq_pool *pool;		 

	union {
		struct snd_seq_user_client user;
		struct snd_seq_kernel_client kernel;
	} data;

	 
	void **ump_info;
};

 
struct snd_seq_usage {
	int cur;
	int peak;
};


int client_init_data(void);
int snd_sequencer_device_init(void);
void snd_sequencer_device_done(void);

 
struct snd_seq_client *snd_seq_client_use_ptr(int clientid);

 
#define snd_seq_client_unlock(client) snd_use_lock_free(&(client)->use_lock)

 
int snd_seq_dispatch_event(struct snd_seq_event_cell *cell, int atomic, int hop);

int snd_seq_kernel_client_write_poll(int clientid, struct file *file, poll_table *wait);
int snd_seq_client_notify_subscription(int client, int port,
				       struct snd_seq_port_subscribe *info, int evtype);

int __snd_seq_deliver_single_event(struct snd_seq_client *dest,
				   struct snd_seq_client_port *dest_port,
				   struct snd_seq_event *event,
				   int atomic, int hop);

 
bool snd_seq_client_ioctl_lock(int clientid);
void snd_seq_client_ioctl_unlock(int clientid);

extern int seq_client_load[15];

 
struct snd_seq_client *snd_seq_kernel_client_get(int client);
void snd_seq_kernel_client_put(struct snd_seq_client *cptr);

static inline bool snd_seq_client_is_ump(struct snd_seq_client *c)
{
	return c->midi_version != SNDRV_SEQ_CLIENT_LEGACY_MIDI;
}

static inline bool snd_seq_client_is_midi2(struct snd_seq_client *c)
{
	return c->midi_version == SNDRV_SEQ_CLIENT_UMP_MIDI_2_0;
}

#endif
