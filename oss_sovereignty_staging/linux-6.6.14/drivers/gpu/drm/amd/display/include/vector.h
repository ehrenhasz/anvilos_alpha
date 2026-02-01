 

#ifndef __DAL_VECTOR_H__
#define __DAL_VECTOR_H__

struct vector {
	uint8_t *container;
	uint32_t struct_size;
	uint32_t count;
	uint32_t capacity;
	struct dc_context *ctx;
};

bool dal_vector_construct(
	struct vector *vector,
	struct dc_context *ctx,
	uint32_t capacity,
	uint32_t struct_size);

struct vector *dal_vector_create(
	struct dc_context *ctx,
	uint32_t capacity,
	uint32_t struct_size);

 
struct vector *dal_vector_presized_create(
	struct dc_context *ctx,
	uint32_t size,
	void *initial_value,
	uint32_t struct_size);

void dal_vector_destruct(
	struct vector *vector);

void dal_vector_destroy(
	struct vector **vector);

uint32_t dal_vector_get_count(
	const struct vector *vector);

 
bool dal_vector_insert_at(
	struct vector *vector,
	const void *what,
	uint32_t position);

bool dal_vector_append(
	struct vector *vector,
	const void *item);

 
void *dal_vector_at_index(
	const struct vector *vector,
	uint32_t index);

void dal_vector_set_at_index(
	const struct vector *vector,
	const void *what,
	uint32_t index);

 
struct vector *dal_vector_clone(
	const struct vector *vector_other);

 
bool dal_vector_remove_at_index(
	struct vector *vector,
	uint32_t index);

uint32_t dal_vector_capacity(const struct vector *vector);

bool dal_vector_reserve(struct vector *vector, uint32_t capacity);

void dal_vector_clear(struct vector *vector);

 

#define DAL_VECTOR_INSERT_AT(vector_type, type_t) \
	static bool vector_type##_vector_insert_at( \
		struct vector *vector, \
		type_t what, \
		uint32_t position) \
{ \
	return dal_vector_insert_at(vector, what, position); \
}

#define DAL_VECTOR_APPEND(vector_type, type_t) \
	static bool vector_type##_vector_append( \
		struct vector *vector, \
		type_t item) \
{ \
	return dal_vector_append(vector, item); \
}

 
#define DAL_VECTOR_AT_INDEX(vector_type, type_t) \
	static type_t vector_type##_vector_at_index( \
		const struct vector *vector, \
		uint32_t index) \
{ \
	return dal_vector_at_index(vector, index); \
}

#define DAL_VECTOR_SET_AT_INDEX(vector_type, type_t) \
	static void vector_type##_vector_set_at_index( \
		const struct vector *vector, \
		type_t what, \
		uint32_t index) \
{ \
	dal_vector_set_at_index(vector, what, index); \
}

#endif  
