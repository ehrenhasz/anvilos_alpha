#include	<sys/bqueue.h>
#include	<sys/zfs_context.h>
static inline bqueue_node_t *
obj2node(bqueue_t *q, void *data)
{
	return ((bqueue_node_t *)((char *)data + q->bq_node_offset));
}
int
bqueue_init(bqueue_t *q, uint_t fill_fraction, size_t size, size_t node_offset)
{
	if (fill_fraction == 0) {
		return (-1);
	}
	list_create(&q->bq_list, node_offset + sizeof (bqueue_node_t),
	    node_offset + offsetof(bqueue_node_t, bqn_node));
	list_create(&q->bq_dequeuing_list, node_offset + sizeof (bqueue_node_t),
	    node_offset + offsetof(bqueue_node_t, bqn_node));
	list_create(&q->bq_enqueuing_list, node_offset + sizeof (bqueue_node_t),
	    node_offset + offsetof(bqueue_node_t, bqn_node));
	cv_init(&q->bq_add_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&q->bq_pop_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&q->bq_lock, NULL, MUTEX_DEFAULT, NULL);
	q->bq_node_offset = node_offset;
	q->bq_size = 0;
	q->bq_dequeuing_size = 0;
	q->bq_enqueuing_size = 0;
	q->bq_maxsize = size;
	q->bq_fill_fraction = fill_fraction;
	return (0);
}
void
bqueue_destroy(bqueue_t *q)
{
	mutex_enter(&q->bq_lock);
	ASSERT0(q->bq_size);
	ASSERT0(q->bq_dequeuing_size);
	ASSERT0(q->bq_enqueuing_size);
	cv_destroy(&q->bq_add_cv);
	cv_destroy(&q->bq_pop_cv);
	list_destroy(&q->bq_list);
	list_destroy(&q->bq_dequeuing_list);
	list_destroy(&q->bq_enqueuing_list);
	mutex_exit(&q->bq_lock);
	mutex_destroy(&q->bq_lock);
}
static void
bqueue_enqueue_impl(bqueue_t *q, void *data, size_t item_size, boolean_t flush)
{
	ASSERT3U(item_size, >, 0);
	ASSERT3U(item_size, <=, q->bq_maxsize);
	obj2node(q, data)->bqn_size = item_size;
	q->bq_enqueuing_size += item_size;
	list_insert_tail(&q->bq_enqueuing_list, data);
	if (flush ||
	    q->bq_enqueuing_size >= q->bq_maxsize / q->bq_fill_fraction) {
		mutex_enter(&q->bq_lock);
		while (q->bq_size > q->bq_maxsize) {
			cv_wait_sig(&q->bq_add_cv, &q->bq_lock);
		}
		q->bq_size += q->bq_enqueuing_size;
		list_move_tail(&q->bq_list, &q->bq_enqueuing_list);
		q->bq_enqueuing_size = 0;
		cv_broadcast(&q->bq_pop_cv);
		mutex_exit(&q->bq_lock);
	}
}
void
bqueue_enqueue(bqueue_t *q, void *data, size_t item_size)
{
	bqueue_enqueue_impl(q, data, item_size, B_FALSE);
}
void
bqueue_enqueue_flush(bqueue_t *q, void *data, size_t item_size)
{
	bqueue_enqueue_impl(q, data, item_size, B_TRUE);
}
void *
bqueue_dequeue(bqueue_t *q)
{
	void *ret = list_remove_head(&q->bq_dequeuing_list);
	if (ret == NULL) {
		mutex_enter(&q->bq_lock);
		while (q->bq_size == 0) {
			cv_wait_sig(&q->bq_pop_cv, &q->bq_lock);
		}
		ASSERT0(q->bq_dequeuing_size);
		ASSERT(list_is_empty(&q->bq_dequeuing_list));
		list_move_tail(&q->bq_dequeuing_list, &q->bq_list);
		q->bq_dequeuing_size = q->bq_size;
		q->bq_size = 0;
		cv_broadcast(&q->bq_add_cv);
		mutex_exit(&q->bq_lock);
		ret = list_remove_head(&q->bq_dequeuing_list);
	}
	q->bq_dequeuing_size -= obj2node(q, ret)->bqn_size;
	return (ret);
}
