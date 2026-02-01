
 

#include "decompress_common.h"

 
int make_huffman_decode_table(u16 decode_table[], const u32 num_syms,
			      const u32 table_bits, const u8 lens[],
			      const u32 max_codeword_len,
			      u16 working_space[])
{
	const u32 table_num_entries = 1 << table_bits;
	u16 * const len_counts = &working_space[0];
	u16 * const offsets = &working_space[1 * (max_codeword_len + 1)];
	u16 * const sorted_syms = &working_space[2 * (max_codeword_len + 1)];
	int left;
	void *decode_table_ptr;
	u32 sym_idx;
	u32 codeword_len;
	u32 stores_per_loop;
	u32 decode_table_pos;
	u32 len;
	u32 sym;

	 
	for (len = 0; len <= max_codeword_len; len++)
		len_counts[len] = 0;
	for (sym = 0; sym < num_syms; sym++)
		len_counts[lens[sym]]++;

	 
	left = 1;
	for (len = 1; len <= max_codeword_len; len++) {
		left <<= 1;
		left -= len_counts[len];
		if (left < 0) {
			 
			return -1;
		}
	}

	if (left) {
		 
		if (left == (1 << max_codeword_len)) {
			 
			memset(decode_table, 0,
			       table_num_entries * sizeof(decode_table[0]));
			return 0;
		}
		return -1;
	}

	 

	 
	offsets[1] = 0;
	for (len = 1; len < max_codeword_len; len++)
		offsets[len + 1] = offsets[len] + len_counts[len];

	 
	for (sym = 0; sym < num_syms; sym++)
		if (lens[sym])
			sorted_syms[offsets[lens[sym]]++] = sym;

	 
	decode_table_ptr = decode_table;
	sym_idx = 0;
	codeword_len = 1;
	stores_per_loop = (1 << (table_bits - codeword_len));
	for (; stores_per_loop != 0; codeword_len++, stores_per_loop >>= 1) {
		u32 end_sym_idx = sym_idx + len_counts[codeword_len];

		for (; sym_idx < end_sym_idx; sym_idx++) {
			u16 entry;
			u16 *p;
			u32 n;

			entry = ((u32)codeword_len << 11) | sorted_syms[sym_idx];
			p = (u16 *)decode_table_ptr;
			n = stores_per_loop;

			do {
				*p++ = entry;
			} while (--n);

			decode_table_ptr = p;
		}
	}

	 
	decode_table_pos = (u16 *)decode_table_ptr - decode_table;
	if (decode_table_pos != table_num_entries) {
		u32 j;
		u32 next_free_tree_slot;
		u32 cur_codeword;

		 
		j = decode_table_pos;
		do {
			decode_table[j] = 0;
		} while (++j != table_num_entries);

		 
		next_free_tree_slot = table_num_entries;

		 
		for (cur_codeword = decode_table_pos << 1;
		     codeword_len <= max_codeword_len;
		     codeword_len++, cur_codeword <<= 1) {
			u32 end_sym_idx = sym_idx + len_counts[codeword_len];

			for (; sym_idx < end_sym_idx; sym_idx++, cur_codeword++) {
				 
				u32 sorted_sym = sorted_syms[sym_idx];
				u32 extra_bits = codeword_len - table_bits;
				u32 node_idx = cur_codeword >> extra_bits;

				 
				do {
					 
					if (decode_table[node_idx] == 0) {
						decode_table[node_idx] =
							next_free_tree_slot | 0xC000;
						decode_table[next_free_tree_slot++] = 0;
						decode_table[next_free_tree_slot++] = 0;
					}

					 
					node_idx = decode_table[node_idx] & 0x3FFF;
					--extra_bits;
					node_idx += (cur_codeword >> extra_bits) & 1;
				} while (extra_bits != 0);

				 
				decode_table[node_idx] = sorted_sym;
			}
		}
	}
	return 0;
}
