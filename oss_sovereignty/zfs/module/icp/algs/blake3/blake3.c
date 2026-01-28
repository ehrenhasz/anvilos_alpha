#include <sys/simd.h>
#include <sys/zfs_context.h>
#include <sys/blake3.h>
#include "blake3_impl.h"
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#endif
typedef struct {
	uint32_t input_cv[8];
	uint64_t counter;
	uint8_t block[BLAKE3_BLOCK_LEN];
	uint8_t block_len;
	uint8_t flags;
} output_t;
enum blake3_flags {
	CHUNK_START		= 1 << 0,
	CHUNK_END		= 1 << 1,
	PARENT			= 1 << 2,
	ROOT			= 1 << 3,
	KEYED_HASH		= 1 << 4,
	DERIVE_KEY_CONTEXT	= 1 << 5,
	DERIVE_KEY_MATERIAL	= 1 << 6,
};
static void chunk_state_init(blake3_chunk_state_t *ctx,
    const uint32_t key[8], uint8_t flags)
{
	memcpy(ctx->cv, key, BLAKE3_KEY_LEN);
	ctx->chunk_counter = 0;
	memset(ctx->buf, 0, BLAKE3_BLOCK_LEN);
	ctx->buf_len = 0;
	ctx->blocks_compressed = 0;
	ctx->flags = flags;
}
static void chunk_state_reset(blake3_chunk_state_t *ctx,
    const uint32_t key[8], uint64_t chunk_counter)
{
	memcpy(ctx->cv, key, BLAKE3_KEY_LEN);
	ctx->chunk_counter = chunk_counter;
	ctx->blocks_compressed = 0;
	memset(ctx->buf, 0, BLAKE3_BLOCK_LEN);
	ctx->buf_len = 0;
}
static size_t chunk_state_len(const blake3_chunk_state_t *ctx)
{
	return (BLAKE3_BLOCK_LEN * (size_t)ctx->blocks_compressed) +
	    ((size_t)ctx->buf_len);
}
static size_t chunk_state_fill_buf(blake3_chunk_state_t *ctx,
    const uint8_t *input, size_t input_len)
{
	size_t take = BLAKE3_BLOCK_LEN - ((size_t)ctx->buf_len);
	if (take > input_len) {
		take = input_len;
	}
	uint8_t *dest = ctx->buf + ((size_t)ctx->buf_len);
	memcpy(dest, input, take);
	ctx->buf_len += (uint8_t)take;
	return (take);
}
static uint8_t chunk_state_maybe_start_flag(const blake3_chunk_state_t *ctx)
{
	if (ctx->blocks_compressed == 0) {
		return (CHUNK_START);
	} else {
		return (0);
	}
}
static output_t make_output(const uint32_t input_cv[8],
    const uint8_t *block, uint8_t block_len,
    uint64_t counter, uint8_t flags)
{
	output_t ret;
	memcpy(ret.input_cv, input_cv, 32);
	memcpy(ret.block, block, BLAKE3_BLOCK_LEN);
	ret.block_len = block_len;
	ret.counter = counter;
	ret.flags = flags;
	return (ret);
}
static void output_chaining_value(const blake3_ops_t *ops,
    const output_t *ctx, uint8_t cv[32])
{
	uint32_t cv_words[8];
	memcpy(cv_words, ctx->input_cv, 32);
	ops->compress_in_place(cv_words, ctx->block, ctx->block_len,
	    ctx->counter, ctx->flags);
	store_cv_words(cv, cv_words);
}
static void output_root_bytes(const blake3_ops_t *ops, const output_t *ctx,
    uint64_t seek, uint8_t *out, size_t out_len)
{
	uint64_t output_block_counter = seek / 64;
	size_t offset_within_block = seek % 64;
	uint8_t wide_buf[64];
	while (out_len > 0) {
		ops->compress_xof(ctx->input_cv, ctx->block, ctx->block_len,
		    output_block_counter, ctx->flags | ROOT, wide_buf);
		size_t available_bytes = 64 - offset_within_block;
		size_t memcpy_len;
		if (out_len > available_bytes) {
			memcpy_len = available_bytes;
		} else {
			memcpy_len = out_len;
		}
		memcpy(out, wide_buf + offset_within_block, memcpy_len);
		out += memcpy_len;
		out_len -= memcpy_len;
		output_block_counter += 1;
		offset_within_block = 0;
	}
}
static void chunk_state_update(const blake3_ops_t *ops,
    blake3_chunk_state_t *ctx, const uint8_t *input, size_t input_len)
{
	if (ctx->buf_len > 0) {
		size_t take = chunk_state_fill_buf(ctx, input, input_len);
		input += take;
		input_len -= take;
		if (input_len > 0) {
			ops->compress_in_place(ctx->cv, ctx->buf,
			    BLAKE3_BLOCK_LEN, ctx->chunk_counter,
			    ctx->flags|chunk_state_maybe_start_flag(ctx));
			ctx->blocks_compressed += 1;
			ctx->buf_len = 0;
			memset(ctx->buf, 0, BLAKE3_BLOCK_LEN);
		}
	}
	while (input_len > BLAKE3_BLOCK_LEN) {
		ops->compress_in_place(ctx->cv, input, BLAKE3_BLOCK_LEN,
		    ctx->chunk_counter,
		    ctx->flags|chunk_state_maybe_start_flag(ctx));
		ctx->blocks_compressed += 1;
		input += BLAKE3_BLOCK_LEN;
		input_len -= BLAKE3_BLOCK_LEN;
	}
	chunk_state_fill_buf(ctx, input, input_len);
}
static output_t chunk_state_output(const blake3_chunk_state_t *ctx)
{
	uint8_t block_flags =
	    ctx->flags | chunk_state_maybe_start_flag(ctx) | CHUNK_END;
	return (make_output(ctx->cv, ctx->buf, ctx->buf_len, ctx->chunk_counter,
	    block_flags));
}
static output_t parent_output(const uint8_t block[BLAKE3_BLOCK_LEN],
    const uint32_t key[8], uint8_t flags)
{
	return (make_output(key, block, BLAKE3_BLOCK_LEN, 0, flags | PARENT));
}
static size_t left_len(size_t content_len)
{
	size_t full_chunks = (content_len - 1) / BLAKE3_CHUNK_LEN;
	return (round_down_to_power_of_2(full_chunks) * BLAKE3_CHUNK_LEN);
}
static size_t compress_chunks_parallel(const blake3_ops_t *ops,
    const uint8_t *input, size_t input_len, const uint32_t key[8],
    uint64_t chunk_counter, uint8_t flags, uint8_t *out)
{
	const uint8_t *chunks_array[MAX_SIMD_DEGREE];
	size_t input_position = 0;
	size_t chunks_array_len = 0;
	while (input_len - input_position >= BLAKE3_CHUNK_LEN) {
		chunks_array[chunks_array_len] = &input[input_position];
		input_position += BLAKE3_CHUNK_LEN;
		chunks_array_len += 1;
	}
	ops->hash_many(chunks_array, chunks_array_len, BLAKE3_CHUNK_LEN /
	    BLAKE3_BLOCK_LEN, key, chunk_counter, B_TRUE, flags, CHUNK_START,
	    CHUNK_END, out);
	if (input_len > input_position) {
		uint64_t counter = chunk_counter + (uint64_t)chunks_array_len;
		blake3_chunk_state_t chunk_state;
		chunk_state_init(&chunk_state, key, flags);
		chunk_state.chunk_counter = counter;
		chunk_state_update(ops, &chunk_state, &input[input_position],
		    input_len - input_position);
		output_t output = chunk_state_output(&chunk_state);
		output_chaining_value(ops, &output, &out[chunks_array_len *
		    BLAKE3_OUT_LEN]);
		return (chunks_array_len + 1);
	} else {
		return (chunks_array_len);
	}
}
static size_t compress_parents_parallel(const blake3_ops_t *ops,
    const uint8_t *child_chaining_values, size_t num_chaining_values,
    const uint32_t key[8], uint8_t flags, uint8_t *out)
{
	const uint8_t *parents_array[MAX_SIMD_DEGREE_OR_2] = {0};
	size_t parents_array_len = 0;
	while (num_chaining_values - (2 * parents_array_len) >= 2) {
		parents_array[parents_array_len] = &child_chaining_values[2 *
		    parents_array_len * BLAKE3_OUT_LEN];
		parents_array_len += 1;
	}
	ops->hash_many(parents_array, parents_array_len, 1, key, 0, B_FALSE,
	    flags | PARENT, 0, 0, out);
	if (num_chaining_values > 2 * parents_array_len) {
		memcpy(&out[parents_array_len * BLAKE3_OUT_LEN],
		    &child_chaining_values[2 * parents_array_len *
		    BLAKE3_OUT_LEN], BLAKE3_OUT_LEN);
		return (parents_array_len + 1);
	} else {
		return (parents_array_len);
	}
}
static size_t blake3_compress_subtree_wide(const blake3_ops_t *ops,
    const uint8_t *input, size_t input_len, const uint32_t key[8],
    uint64_t chunk_counter, uint8_t flags, uint8_t *out)
{
	if (input_len <= (size_t)(ops->degree * BLAKE3_CHUNK_LEN)) {
		return (compress_chunks_parallel(ops, input, input_len, key,
		    chunk_counter, flags, out));
	}
	size_t left_input_len = left_len(input_len);
	size_t right_input_len = input_len - left_input_len;
	const uint8_t *right_input = &input[left_input_len];
	uint64_t right_chunk_counter = chunk_counter +
	    (uint64_t)(left_input_len / BLAKE3_CHUNK_LEN);
	uint8_t cv_array[2 * MAX_SIMD_DEGREE_OR_2 * BLAKE3_OUT_LEN];
	size_t degree = ops->degree;
	if (left_input_len > BLAKE3_CHUNK_LEN && degree == 1) {
		degree = 2;
	}
	uint8_t *right_cvs = &cv_array[degree * BLAKE3_OUT_LEN];
	size_t left_n = blake3_compress_subtree_wide(ops, input, left_input_len,
	    key, chunk_counter, flags, cv_array);
	size_t right_n = blake3_compress_subtree_wide(ops, right_input,
	    right_input_len, key, right_chunk_counter, flags, right_cvs);
	if (left_n == 1) {
		memcpy(out, cv_array, 2 * BLAKE3_OUT_LEN);
		return (2);
	}
	size_t num_chaining_values = left_n + right_n;
	return compress_parents_parallel(ops, cv_array,
	    num_chaining_values, key, flags, out);
}
static void compress_subtree_to_parent_node(const blake3_ops_t *ops,
    const uint8_t *input, size_t input_len, const uint32_t key[8],
    uint64_t chunk_counter, uint8_t flags, uint8_t out[2 * BLAKE3_OUT_LEN])
{
	uint8_t cv_array[MAX_SIMD_DEGREE_OR_2 * BLAKE3_OUT_LEN];
	size_t num_cvs = blake3_compress_subtree_wide(ops, input, input_len,
	    key, chunk_counter, flags, cv_array);
	uint8_t out_array[MAX_SIMD_DEGREE_OR_2 * BLAKE3_OUT_LEN / 2];
	while (num_cvs > 2) {
		num_cvs = compress_parents_parallel(ops, cv_array, num_cvs, key,
		    flags, out_array);
		memcpy(cv_array, out_array, num_cvs * BLAKE3_OUT_LEN);
	}
	memcpy(out, cv_array, 2 * BLAKE3_OUT_LEN);
}
static void hasher_init_base(BLAKE3_CTX *ctx, const uint32_t key[8],
    uint8_t flags)
{
	memcpy(ctx->key, key, BLAKE3_KEY_LEN);
	chunk_state_init(&ctx->chunk, key, flags);
	ctx->cv_stack_len = 0;
	ctx->ops = blake3_get_ops();
}
static void hasher_merge_cv_stack(BLAKE3_CTX *ctx, uint64_t total_len)
{
	size_t post_merge_stack_len = (size_t)popcnt(total_len);
	while (ctx->cv_stack_len > post_merge_stack_len) {
		uint8_t *parent_node =
		    &ctx->cv_stack[(ctx->cv_stack_len - 2) * BLAKE3_OUT_LEN];
		output_t output =
		    parent_output(parent_node, ctx->key, ctx->chunk.flags);
		output_chaining_value(ctx->ops, &output, parent_node);
		ctx->cv_stack_len -= 1;
	}
}
static void hasher_push_cv(BLAKE3_CTX *ctx, uint8_t new_cv[BLAKE3_OUT_LEN],
    uint64_t chunk_counter)
{
	hasher_merge_cv_stack(ctx, chunk_counter);
	memcpy(&ctx->cv_stack[ctx->cv_stack_len * BLAKE3_OUT_LEN], new_cv,
	    BLAKE3_OUT_LEN);
	ctx->cv_stack_len += 1;
}
void
Blake3_Init(BLAKE3_CTX *ctx)
{
	hasher_init_base(ctx, BLAKE3_IV, 0);
}
void
Blake3_InitKeyed(BLAKE3_CTX *ctx, const uint8_t key[BLAKE3_KEY_LEN])
{
	uint32_t key_words[8];
	load_key_words(key, key_words);
	hasher_init_base(ctx, key_words, KEYED_HASH);
}
static void
Blake3_Update2(BLAKE3_CTX *ctx, const void *input, size_t input_len)
{
	if (input_len == 0) {
		return;
	}
	const uint8_t *input_bytes = (const uint8_t *)input;
	if (chunk_state_len(&ctx->chunk) > 0) {
		size_t take = BLAKE3_CHUNK_LEN - chunk_state_len(&ctx->chunk);
		if (take > input_len) {
			take = input_len;
		}
		chunk_state_update(ctx->ops, &ctx->chunk, input_bytes, take);
		input_bytes += take;
		input_len -= take;
		if (input_len > 0) {
			output_t output = chunk_state_output(&ctx->chunk);
			uint8_t chunk_cv[32];
			output_chaining_value(ctx->ops, &output, chunk_cv);
			hasher_push_cv(ctx, chunk_cv, ctx->chunk.chunk_counter);
			chunk_state_reset(&ctx->chunk, ctx->key,
			    ctx->chunk.chunk_counter + 1);
		} else {
			return;
		}
	}
	while (input_len > BLAKE3_CHUNK_LEN) {
		size_t subtree_len = round_down_to_power_of_2(input_len);
		uint64_t count_so_far =
		    ctx->chunk.chunk_counter * BLAKE3_CHUNK_LEN;
		while ((((uint64_t)(subtree_len - 1)) & count_so_far) != 0) {
			subtree_len /= 2;
		}
		uint64_t subtree_chunks = subtree_len / BLAKE3_CHUNK_LEN;
		if (subtree_len <= BLAKE3_CHUNK_LEN) {
			blake3_chunk_state_t chunk_state;
			chunk_state_init(&chunk_state, ctx->key,
			    ctx->chunk.flags);
			chunk_state.chunk_counter = ctx->chunk.chunk_counter;
			chunk_state_update(ctx->ops, &chunk_state, input_bytes,
			    subtree_len);
			output_t output = chunk_state_output(&chunk_state);
			uint8_t cv[BLAKE3_OUT_LEN];
			output_chaining_value(ctx->ops, &output, cv);
			hasher_push_cv(ctx, cv, chunk_state.chunk_counter);
		} else {
			uint8_t cv_pair[2 * BLAKE3_OUT_LEN];
			compress_subtree_to_parent_node(ctx->ops, input_bytes,
			    subtree_len, ctx->key, ctx-> chunk.chunk_counter,
			    ctx->chunk.flags, cv_pair);
			hasher_push_cv(ctx, cv_pair, ctx->chunk.chunk_counter);
			hasher_push_cv(ctx, &cv_pair[BLAKE3_OUT_LEN],
			    ctx->chunk.chunk_counter + (subtree_chunks / 2));
		}
		ctx->chunk.chunk_counter += subtree_chunks;
		input_bytes += subtree_len;
		input_len -= subtree_len;
	}
	if (input_len > 0) {
		chunk_state_update(ctx->ops, &ctx->chunk, input_bytes,
		    input_len);
		hasher_merge_cv_stack(ctx, ctx->chunk.chunk_counter);
	}
}
void
Blake3_Update(BLAKE3_CTX *ctx, const void *input, size_t todo)
{
	size_t done = 0;
	const uint8_t *data = input;
	const size_t block_max = 1024 * 64;
	while (todo != 0) {
		size_t block = (todo >= block_max) ? block_max : todo;
		Blake3_Update2(ctx, data + done, block);
		done += block;
		todo -= block;
	}
}
void
Blake3_Final(const BLAKE3_CTX *ctx, uint8_t *out)
{
	Blake3_FinalSeek(ctx, 0, out, BLAKE3_OUT_LEN);
}
void
Blake3_FinalSeek(const BLAKE3_CTX *ctx, uint64_t seek, uint8_t *out,
    size_t out_len)
{
	if (out_len == 0) {
		return;
	}
	if (ctx->cv_stack_len == 0) {
		output_t output = chunk_state_output(&ctx->chunk);
		output_root_bytes(ctx->ops, &output, seek, out, out_len);
		return;
	}
	output_t output;
	size_t cvs_remaining;
	if (chunk_state_len(&ctx->chunk) > 0) {
		cvs_remaining = ctx->cv_stack_len;
		output = chunk_state_output(&ctx->chunk);
	} else {
		cvs_remaining = ctx->cv_stack_len - 2;
		output = parent_output(&ctx->cv_stack[cvs_remaining * 32],
		    ctx->key, ctx->chunk.flags);
	}
	while (cvs_remaining > 0) {
		cvs_remaining -= 1;
		uint8_t parent_block[BLAKE3_BLOCK_LEN];
		memcpy(parent_block, &ctx->cv_stack[cvs_remaining * 32], 32);
		output_chaining_value(ctx->ops, &output, &parent_block[32]);
		output = parent_output(parent_block, ctx->key,
		    ctx->chunk.flags);
	}
	output_root_bytes(ctx->ops, &output, seek, out, out_len);
}
