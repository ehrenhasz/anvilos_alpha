 

#include "uzlib.h"

#include "defl_static.c"

#define MATCH_LEN_MIN (3)
#define MATCH_LEN_MAX (258)





void uzlib_lz77_init(uzlib_lz77_state_t *state, uint8_t *hist, size_t hist_max) {
    memset(state, 0, sizeof(uzlib_lz77_state_t));
    state->hist_buf = hist;
    state->hist_max = hist_max;
    state->hist_start = 0;
    state->hist_len = 0;
}




static size_t uzlib_lz77_search_max_match(uzlib_lz77_state_t *state, const uint8_t *src, size_t len, size_t *longest_offset) {
    size_t longest_len = 0;
    for (size_t hist_search = 0; hist_search < state->hist_len; ++hist_search) {
        
        size_t match_len;
        for (match_len = 0; match_len <= MATCH_LEN_MAX && match_len < len; ++match_len) {
            uint8_t hist;
            if (hist_search + match_len < state->hist_len) {
                hist = state->hist_buf[(state->hist_start + hist_search + match_len) & (state->hist_max - 1)];
            } else {
                hist = src[hist_search + match_len - state->hist_len];
            }
            if (src[match_len] != hist) {
                break;
            }
        }

        
        
        
        if (match_len >= MATCH_LEN_MIN && match_len >= longest_len) {
            longest_len = match_len;
            *longest_offset = state->hist_len - hist_search;
        }
    }

    return longest_len;
}


void uzlib_lz77_compress(uzlib_lz77_state_t *state, const uint8_t *src, unsigned len) {
    const uint8_t *top = src + len;
    while (src < top) {
        
        size_t match_offset = 0;
        size_t match_len = uzlib_lz77_search_max_match(state, src, top - src, &match_offset);

        
        if (match_len == 0) {
            uzlib_literal(state, *src);
            match_len = 1;
        } else {
            uzlib_match(state, match_offset, match_len);
        }

        
        size_t mask = state->hist_max - 1;
        while (match_len--) {
            uint8_t b = *src++;
            state->hist_buf[(state->hist_start + state->hist_len) & mask] = b;
            if (state->hist_len == state->hist_max) {
                state->hist_start = (state->hist_start + 1) & mask;
            } else {
                ++state->hist_len;
            }
        }
    }
}
