 

#include "dm_services.h"
#include "include/vector.h"

bool dal_vector_construct(
	struct vector *vector,
	struct dc_context *ctx,
	uint32_t capacity,
	uint32_t struct_size)
{
	vector->container = NULL;

	if (!struct_size || !capacity) {
		 
		BREAK_TO_DEBUGGER();
		return false;
	}

	vector->container = kcalloc(capacity, struct_size, GFP_KERNEL);
	if (vector->container == NULL)
		return false;
	vector->capacity = capacity;
	vector->struct_size = struct_size;
	vector->count = 0;
	vector->ctx = ctx;
	return true;
}

static bool dal_vector_presized_costruct(struct vector *vector,
					 struct dc_context *ctx,
					 uint32_t count,
					 void *initial_value,
					 uint32_t struct_size)
{
	uint32_t i;

	vector->container = NULL;

	if (!struct_size || !count) {
		 
		BREAK_TO_DEBUGGER();
		return false;
	}

	vector->container = kcalloc(count, struct_size, GFP_KERNEL);

	if (vector->container == NULL)
		return false;

	 
	if (NULL != initial_value) {
		for (i = 0; i < count; ++i)
			memmove(
				vector->container + i * struct_size,
				initial_value,
				struct_size);
	}

	vector->capacity = count;
	vector->struct_size = struct_size;
	vector->count = count;
	return true;
}

struct vector *dal_vector_presized_create(
	struct dc_context *ctx,
	uint32_t size,
	void *initial_value,
	uint32_t struct_size)
{
	struct vector *vector = kzalloc(sizeof(struct vector), GFP_KERNEL);

	if (vector == NULL)
		return NULL;

	if (dal_vector_presized_costruct(
		vector, ctx, size, initial_value, struct_size))
		return vector;

	BREAK_TO_DEBUGGER();
	kfree(vector);
	return NULL;
}

struct vector *dal_vector_create(
	struct dc_context *ctx,
	uint32_t capacity,
	uint32_t struct_size)
{
	struct vector *vector = kzalloc(sizeof(struct vector), GFP_KERNEL);

	if (vector == NULL)
		return NULL;

	if (dal_vector_construct(vector, ctx, capacity, struct_size))
		return vector;

	BREAK_TO_DEBUGGER();
	kfree(vector);
	return NULL;
}

void dal_vector_destruct(
	struct vector *vector)
{
	kfree(vector->container);
	vector->count = 0;
	vector->capacity = 0;
}

void dal_vector_destroy(
	struct vector **vector)
{
	if (vector == NULL || *vector == NULL)
		return;
	dal_vector_destruct(*vector);
	kfree(*vector);
	*vector = NULL;
}

uint32_t dal_vector_get_count(
	const struct vector *vector)
{
	return vector->count;
}

void *dal_vector_at_index(
	const struct vector *vector,
	uint32_t index)
{
	if (vector->container == NULL || index >= vector->count)
		return NULL;
	return vector->container + (index * vector->struct_size);
}

bool dal_vector_remove_at_index(
	struct vector *vector,
	uint32_t index)
{
	if (index >= vector->count)
		return false;

	if (index != vector->count - 1)
		memmove(
			vector->container + (index * vector->struct_size),
			vector->container + ((index + 1) * vector->struct_size),
			(vector->count - index - 1) * vector->struct_size);
	vector->count -= 1;

	return true;
}

void dal_vector_set_at_index(
	const struct vector *vector,
	const void *what,
	uint32_t index)
{
	void *where = dal_vector_at_index(vector, index);

	if (!where) {
		BREAK_TO_DEBUGGER();
		return;
	}
	memmove(
		where,
		what,
		vector->struct_size);
}

static inline uint32_t calc_increased_capacity(
	uint32_t old_capacity)
{
	return old_capacity * 2;
}

bool dal_vector_insert_at(
	struct vector *vector,
	const void *what,
	uint32_t position)
{
	uint8_t *insert_address;

	if (vector->count == vector->capacity) {
		if (!dal_vector_reserve(
			vector,
			calc_increased_capacity(vector->capacity)))
			return false;
	}

	insert_address = vector->container + (vector->struct_size * position);

	if (vector->count && position < vector->count)
		memmove(
			insert_address + vector->struct_size,
			insert_address,
			vector->struct_size * (vector->count - position));

	memmove(
		insert_address,
		what,
		vector->struct_size);

	vector->count++;

	return true;
}

bool dal_vector_append(
	struct vector *vector,
	const void *item)
{
	return dal_vector_insert_at(vector, item, vector->count);
}

struct vector *dal_vector_clone(
	const struct vector *vector)
{
	struct vector *vec_cloned;
	uint32_t count;

	 
	count = dal_vector_get_count(vector);

	if (count == 0)
		 
		vec_cloned = dal_vector_create(
			vector->ctx,
			vector->capacity,
			vector->struct_size);
	else
		 
		vec_cloned = dal_vector_presized_create(vector->ctx, count,
			NULL, 
			vector->struct_size);

	if (NULL == vec_cloned) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	 
	memmove(vec_cloned->container, vector->container,
			vec_cloned->struct_size * vec_cloned->capacity);

	return vec_cloned;
}

uint32_t dal_vector_capacity(const struct vector *vector)
{
	return vector->capacity;
}

bool dal_vector_reserve(struct vector *vector, uint32_t capacity)
{
	void *new_container;

	if (capacity <= vector->capacity)
		return true;

	new_container = krealloc(vector->container,
				 capacity * vector->struct_size, GFP_KERNEL);

	if (new_container) {
		vector->container = new_container;
		vector->capacity = capacity;
		return true;
	}

	return false;
}

void dal_vector_clear(struct vector *vector)
{
	vector->count = 0;
}
