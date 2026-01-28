#ifndef _NET_BATMAN_ADV_BITARRAY_H_
#define _NET_BATMAN_ADV_BITARRAY_H_
#include "main.h"
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/stddef.h>
#include <linux/types.h>
static inline bool batadv_test_bit(const unsigned long *seq_bits,
				   u32 last_seqno, u32 curr_seqno)
{
	s32 diff;
	diff = last_seqno - curr_seqno;
	if (diff < 0 || diff >= BATADV_TQ_LOCAL_WINDOW_SIZE)
		return false;
	return test_bit(diff, seq_bits) != 0;
}
static inline void batadv_set_bit(unsigned long *seq_bits, s32 n)
{
	if (n < 0 || n >= BATADV_TQ_LOCAL_WINDOW_SIZE)
		return;
	set_bit(n, seq_bits);  
}
bool batadv_bit_get_packet(void *priv, unsigned long *seq_bits,
			   s32 seq_num_diff, int set_mark);
#endif  
