 
 

#ifndef RXE_QUEUE_H
#define RXE_QUEUE_H

 

 
enum queue_type {
	QUEUE_TYPE_TO_CLIENT,
	QUEUE_TYPE_FROM_CLIENT,
	QUEUE_TYPE_FROM_ULP,
	QUEUE_TYPE_TO_ULP,
};

struct rxe_queue_buf;

struct rxe_queue {
	struct rxe_dev		*rxe;
	struct rxe_queue_buf	*buf;
	struct rxe_mmap_info	*ip;
	size_t			buf_size;
	size_t			elem_size;
	unsigned int		log2_elem_size;
	u32			index_mask;
	enum queue_type		type;
	 
	u32			index;
};

int do_mmap_info(struct rxe_dev *rxe, struct mminfo __user *outbuf,
		 struct ib_udata *udata, struct rxe_queue_buf *buf,
		 size_t buf_size, struct rxe_mmap_info **ip_p);

void rxe_queue_reset(struct rxe_queue *q);

struct rxe_queue *rxe_queue_init(struct rxe_dev *rxe, int *num_elem,
			unsigned int elem_size, enum queue_type type);

int rxe_queue_resize(struct rxe_queue *q, unsigned int *num_elem_p,
		     unsigned int elem_size, struct ib_udata *udata,
		     struct mminfo __user *outbuf,
		     spinlock_t *producer_lock, spinlock_t *consumer_lock);

void rxe_queue_cleanup(struct rxe_queue *queue);

static inline u32 queue_next_index(struct rxe_queue *q, int index)
{
	return (index + 1) & q->index_mask;
}

static inline u32 queue_get_producer(const struct rxe_queue *q,
				     enum queue_type type)
{
	u32 prod;

	switch (type) {
	case QUEUE_TYPE_FROM_CLIENT:
		 
		prod = smp_load_acquire(&q->buf->producer_index);
		break;
	case QUEUE_TYPE_TO_CLIENT:
		 
		prod = q->index;
		break;
	case QUEUE_TYPE_FROM_ULP:
		 
		prod = q->buf->producer_index;
		break;
	case QUEUE_TYPE_TO_ULP:
		 
		prod = smp_load_acquire(&q->buf->producer_index);
		break;
	}

	return prod;
}

static inline u32 queue_get_consumer(const struct rxe_queue *q,
				     enum queue_type type)
{
	u32 cons;

	switch (type) {
	case QUEUE_TYPE_FROM_CLIENT:
		 
		cons = q->index;
		break;
	case QUEUE_TYPE_TO_CLIENT:
		 
		cons = smp_load_acquire(&q->buf->consumer_index);
		break;
	case QUEUE_TYPE_FROM_ULP:
		 
		cons = smp_load_acquire(&q->buf->consumer_index);
		break;
	case QUEUE_TYPE_TO_ULP:
		 
		cons = q->buf->consumer_index;
		break;
	}

	return cons;
}

static inline int queue_empty(struct rxe_queue *q, enum queue_type type)
{
	u32 prod = queue_get_producer(q, type);
	u32 cons = queue_get_consumer(q, type);

	return ((prod - cons) & q->index_mask) == 0;
}

static inline int queue_full(struct rxe_queue *q, enum queue_type type)
{
	u32 prod = queue_get_producer(q, type);
	u32 cons = queue_get_consumer(q, type);

	return ((prod + 1 - cons) & q->index_mask) == 0;
}

static inline u32 queue_count(const struct rxe_queue *q,
					enum queue_type type)
{
	u32 prod = queue_get_producer(q, type);
	u32 cons = queue_get_consumer(q, type);

	return (prod - cons) & q->index_mask;
}

static inline void queue_advance_producer(struct rxe_queue *q,
					  enum queue_type type)
{
	u32 prod;

	switch (type) {
	case QUEUE_TYPE_FROM_CLIENT:
		 
		if (WARN_ON(1))
			pr_warn("%s: attempt to advance client index\n",
				__func__);
		break;
	case QUEUE_TYPE_TO_CLIENT:
		 
		prod = q->index;
		prod = (prod + 1) & q->index_mask;
		q->index = prod;
		 
		smp_store_release(&q->buf->producer_index, prod);
		break;
	case QUEUE_TYPE_FROM_ULP:
		 
		prod = q->buf->producer_index;
		prod = (prod + 1) & q->index_mask;
		 
		smp_store_release(&q->buf->producer_index, prod);
		break;
	case QUEUE_TYPE_TO_ULP:
		 
		if (WARN_ON(1))
			pr_warn("%s: attempt to advance driver index\n",
				__func__);
		break;
	}
}

static inline void queue_advance_consumer(struct rxe_queue *q,
					  enum queue_type type)
{
	u32 cons;

	switch (type) {
	case QUEUE_TYPE_FROM_CLIENT:
		 
		cons = (q->index + 1) & q->index_mask;
		q->index = cons;
		 
		smp_store_release(&q->buf->consumer_index, cons);
		break;
	case QUEUE_TYPE_TO_CLIENT:
		 
		if (WARN_ON(1))
			pr_warn("%s: attempt to advance client index\n",
				__func__);
		break;
	case QUEUE_TYPE_FROM_ULP:
		 
		if (WARN_ON(1))
			pr_warn("%s: attempt to advance driver index\n",
				__func__);
		break;
	case QUEUE_TYPE_TO_ULP:
		 
		cons = q->buf->consumer_index;
		cons = (cons + 1) & q->index_mask;
		 
		smp_store_release(&q->buf->consumer_index, cons);
		break;
	}
}

static inline void *queue_producer_addr(struct rxe_queue *q,
					enum queue_type type)
{
	u32 prod = queue_get_producer(q, type);

	return q->buf->data + (prod << q->log2_elem_size);
}

static inline void *queue_consumer_addr(struct rxe_queue *q,
					enum queue_type type)
{
	u32 cons = queue_get_consumer(q, type);

	return q->buf->data + (cons << q->log2_elem_size);
}

static inline void *queue_addr_from_index(struct rxe_queue *q, u32 index)
{
	return q->buf->data + ((index & q->index_mask)
				<< q->log2_elem_size);
}

static inline u32 queue_index_from_addr(const struct rxe_queue *q,
				const void *addr)
{
	return (((u8 *)addr - q->buf->data) >> q->log2_elem_size)
				& q->index_mask;
}

static inline void *queue_head(struct rxe_queue *q, enum queue_type type)
{
	return queue_empty(q, type) ? NULL : queue_consumer_addr(q, type);
}

#endif  
