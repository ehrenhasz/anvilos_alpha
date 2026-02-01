
 
#define _GNU_SOURCE
#include "main.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

 
static inline bool need_event(unsigned short event,
			      unsigned short next,
			      unsigned short prev)
{
	return (unsigned short)(next - event - 1) < (unsigned short)(next - prev);
}

 
#define DESC_HW 0x1

struct desc {
	unsigned short flags;
	unsigned short index;
	unsigned len;
	unsigned long long addr;
};

 
#define HOST_GUEST_PADDING 0x80

 
struct event {
	unsigned short kick_index;
	unsigned char reserved0[HOST_GUEST_PADDING - 2];
	unsigned short call_index;
	unsigned char reserved1[HOST_GUEST_PADDING - 2];
};

struct data {
	void *buf;  
	void *data;
} *data;

struct desc *ring;
struct event *event;

struct guest {
	unsigned avail_idx;
	unsigned last_used_idx;
	unsigned num_free;
	unsigned kicked_avail_idx;
	unsigned char reserved[HOST_GUEST_PADDING - 12];
} guest;

struct host {
	 
	unsigned used_idx;
	unsigned called_used_idx;
	unsigned char reserved[HOST_GUEST_PADDING - 4];
} host;

 
void alloc_ring(void)
{
	int ret;
	int i;

	ret = posix_memalign((void **)&ring, 0x1000, ring_size * sizeof *ring);
	if (ret) {
		perror("Unable to allocate ring buffer.\n");
		exit(3);
	}
	event = calloc(1, sizeof(*event));
	if (!event) {
		perror("Unable to allocate event buffer.\n");
		exit(3);
	}
	guest.avail_idx = 0;
	guest.kicked_avail_idx = -1;
	guest.last_used_idx = 0;
	host.used_idx = 0;
	host.called_used_idx = -1;
	for (i = 0; i < ring_size; ++i) {
		struct desc desc = {
			.index = i,
		};
		ring[i] = desc;
	}
	guest.num_free = ring_size;
	data = calloc(ring_size, sizeof(*data));
	if (!data) {
		perror("Unable to allocate data buffer.\n");
		exit(3);
	}
}

 
int add_inbuf(unsigned len, void *buf, void *datap)
{
	unsigned head, index;

	if (!guest.num_free)
		return -1;

	guest.num_free--;
	head = (ring_size - 1) & (guest.avail_idx++);

	 
	ring[head].addr = (unsigned long)(void*)buf;
	ring[head].len = len;
	 
	barrier();
	index = ring[head].index;
	data[index].buf = buf;
	data[index].data = datap;
	 
	smp_release();
	ring[head].flags = DESC_HW;

	return 0;
}

void *get_buf(unsigned *lenp, void **bufp)
{
	unsigned head = (ring_size - 1) & guest.last_used_idx;
	unsigned index;
	void *datap;

	if (ring[head].flags & DESC_HW)
		return NULL;
	 
	smp_acquire();
	*lenp = ring[head].len;
	index = ring[head].index & (ring_size - 1);
	datap = data[index].data;
	*bufp = data[index].buf;
	data[index].buf = NULL;
	data[index].data = NULL;
	guest.num_free++;
	guest.last_used_idx++;
	return datap;
}

bool used_empty()
{
	unsigned head = (ring_size - 1) & guest.last_used_idx;

	return (ring[head].flags & DESC_HW);
}

void disable_call()
{
	 
}

bool enable_call()
{
	event->call_index = guest.last_used_idx;
	 
	 
	smp_mb();
	return used_empty();
}

void kick_available(void)
{
	bool need;

	 
	 
	smp_mb();
	need = need_event(event->kick_index,
			   guest.avail_idx,
			   guest.kicked_avail_idx);

	guest.kicked_avail_idx = guest.avail_idx;
	if (need)
		kick();
}

 
void disable_kick()
{
	 
}

bool enable_kick()
{
	event->kick_index = host.used_idx;
	 
	smp_mb();
	return avail_empty();
}

bool avail_empty()
{
	unsigned head = (ring_size - 1) & host.used_idx;

	return !(ring[head].flags & DESC_HW);
}

bool use_buf(unsigned *lenp, void **bufp)
{
	unsigned head = (ring_size - 1) & host.used_idx;

	if (!(ring[head].flags & DESC_HW))
		return false;

	 
	 
	smp_acquire();

	 
	ring[head].len--;
	 
	 
	smp_release();
	ring[head].flags = 0;
	host.used_idx++;
	return true;
}

void call_used(void)
{
	bool need;

	 
	 
	smp_mb();

	need = need_event(event->call_index,
			host.used_idx,
			host.called_used_idx);

	host.called_used_idx = host.used_idx;

	if (need)
		call();
}
