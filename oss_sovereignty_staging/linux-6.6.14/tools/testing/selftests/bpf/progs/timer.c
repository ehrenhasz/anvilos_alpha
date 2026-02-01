
 
#include <linux/bpf.h>
#include <time.h>
#include <errno.h>
#include <bpf/bpf_helpers.h>
#include "bpf_tcp_helpers.h"

char _license[] SEC("license") = "GPL";
struct hmap_elem {
	int counter;
	struct bpf_timer timer;
	struct bpf_spin_lock lock;  
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1000);
	__type(key, int);
	__type(value, struct hmap_elem);
} hmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(map_flags, BPF_F_NO_PREALLOC);
	__uint(max_entries, 1000);
	__type(key, int);
	__type(value, struct hmap_elem);
} hmap_malloc SEC(".maps");

struct elem {
	struct bpf_timer t;
};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 2);
	__type(key, int);
	__type(value, struct elem);
} array SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_LRU_HASH);
	__uint(max_entries, 4);
	__type(key, int);
	__type(value, struct elem);
} lru SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, int);
	__type(value, struct elem);
} abs_timer SEC(".maps");

__u64 bss_data;
__u64 abs_data;
__u64 err;
__u64 ok;
__u64 callback_check = 52;
__u64 callback2_check = 52;

#define ARRAY 1
#define HTAB 2
#define HTAB_MALLOC 3
#define LRU 4

 
static int timer_cb1(void *map, int *key, struct bpf_timer *timer)
{
	 
	bss_data += 5;

	 
	if (*key == ARRAY) {
		struct bpf_timer *lru_timer;
		int lru_key = LRU;

		 
		if (bpf_timer_start(timer, 1ull << 35, 0) != 0)
			err |= 1;

		lru_timer = bpf_map_lookup_elem(&lru, &lru_key);
		if (!lru_timer)
			return 0;
		bpf_timer_set_callback(lru_timer, timer_cb1);
		if (bpf_timer_start(lru_timer, 0, 0) != 0)
			err |= 2;
	} else if (*key == LRU) {
		int lru_key, i;

		for (i = LRU + 1;
		     i <= 100   ;
		     i++) {
			struct elem init = {};

			 
			lru_key = i;

			 
			bpf_map_update_elem(map, &lru_key, &init, 0);
			 
			bpf_map_lookup_elem(map, &lru_key);

			 
			if (*key != LRU)
				break;
		}

		 
		if (bpf_timer_cancel(timer) != -EINVAL)
			err |= 4;
		ok |= 1;
	}
	return 0;
}

SEC("fentry/bpf_fentry_test1")
int BPF_PROG2(test1, int, a)
{
	struct bpf_timer *arr_timer, *lru_timer;
	struct elem init = {};
	int lru_key = LRU;
	int array_key = ARRAY;

	arr_timer = bpf_map_lookup_elem(&array, &array_key);
	if (!arr_timer)
		return 0;
	bpf_timer_init(arr_timer, &array, CLOCK_MONOTONIC);

	bpf_map_update_elem(&lru, &lru_key, &init, 0);
	lru_timer = bpf_map_lookup_elem(&lru, &lru_key);
	if (!lru_timer)
		return 0;
	bpf_timer_init(lru_timer, &lru, CLOCK_MONOTONIC);

	bpf_timer_set_callback(arr_timer, timer_cb1);
	bpf_timer_start(arr_timer, 0  , 0);

	 
	array_key = 0;
	arr_timer = bpf_map_lookup_elem(&array, &array_key);
	if (!arr_timer)
		return 0;
	bpf_timer_init(arr_timer, &array, CLOCK_MONOTONIC);
	return 0;
}

 
static int timer_cb2(void *map, int *key, struct hmap_elem *val)
{
	if (*key == HTAB)
		callback_check--;
	else
		callback2_check--;
	if (val->counter > 0 && --val->counter) {
		 
		bpf_timer_start(&val->timer, 1000, 0);
	} else if (*key == HTAB) {
		struct bpf_timer *arr_timer;
		int array_key = ARRAY;

		 
		arr_timer = bpf_map_lookup_elem(&array, &array_key);
		if (!arr_timer)
			return 0;
		if (bpf_timer_cancel(arr_timer) != 1)
			 
			err |= 8;

		 
		if (bpf_timer_cancel(&val->timer) != -EDEADLK)
			err |= 16;

		 
		bpf_map_delete_elem(map, key);

		 
		if (bpf_timer_start(&val->timer, 1000, 0) != -EINVAL)
			err |= 32;
		ok |= 2;
	} else {
		if (*key != HTAB_MALLOC)
			err |= 64;

		 
		if (bpf_timer_cancel(&val->timer) != -EDEADLK)
			err |= 128;

		 
		bpf_map_delete_elem(map, key);

		ok |= 4;
	}
	return 0;
}

int bpf_timer_test(void)
{
	struct hmap_elem *val;
	int key = HTAB, key_malloc = HTAB_MALLOC;

	val = bpf_map_lookup_elem(&hmap, &key);
	if (val) {
		if (bpf_timer_init(&val->timer, &hmap, CLOCK_BOOTTIME) != 0)
			err |= 512;
		bpf_timer_set_callback(&val->timer, timer_cb2);
		bpf_timer_start(&val->timer, 1000, 0);
	}
	val = bpf_map_lookup_elem(&hmap_malloc, &key_malloc);
	if (val) {
		if (bpf_timer_init(&val->timer, &hmap_malloc, CLOCK_BOOTTIME) != 0)
			err |= 1024;
		bpf_timer_set_callback(&val->timer, timer_cb2);
		bpf_timer_start(&val->timer, 1000, 0);
	}
	return 0;
}

SEC("fentry/bpf_fentry_test2")
int BPF_PROG2(test2, int, a, int, b)
{
	struct hmap_elem init = {}, *val;
	int key = HTAB, key_malloc = HTAB_MALLOC;

	init.counter = 10;  
	bpf_map_update_elem(&hmap, &key, &init, 0);
	val = bpf_map_lookup_elem(&hmap, &key);
	if (val)
		bpf_timer_init(&val->timer, &hmap, CLOCK_BOOTTIME);
	 
	bpf_map_update_elem(&hmap, &key, &init, 0);

	bpf_map_update_elem(&hmap_malloc, &key_malloc, &init, 0);
	val = bpf_map_lookup_elem(&hmap_malloc, &key_malloc);
	if (val)
		bpf_timer_init(&val->timer, &hmap_malloc, CLOCK_BOOTTIME);
	 
	bpf_map_update_elem(&hmap_malloc, &key_malloc, &init, 0);

	 
	key = 0;
	bpf_map_update_elem(&hmap, &key, &init, 0);
	val = bpf_map_lookup_elem(&hmap, &key);
	if (val)
		bpf_timer_init(&val->timer, &hmap, CLOCK_BOOTTIME);
	bpf_map_delete_elem(&hmap, &key);
	bpf_map_update_elem(&hmap, &key, &init, 0);
	val = bpf_map_lookup_elem(&hmap, &key);
	if (val)
		bpf_timer_init(&val->timer, &hmap, CLOCK_BOOTTIME);

	 
	key_malloc = 0;
	bpf_map_update_elem(&hmap_malloc, &key_malloc, &init, 0);
	val = bpf_map_lookup_elem(&hmap_malloc, &key_malloc);
	if (val)
		bpf_timer_init(&val->timer, &hmap_malloc, CLOCK_BOOTTIME);
	bpf_map_delete_elem(&hmap_malloc, &key_malloc);
	bpf_map_update_elem(&hmap_malloc, &key_malloc, &init, 0);
	val = bpf_map_lookup_elem(&hmap_malloc, &key_malloc);
	if (val)
		bpf_timer_init(&val->timer, &hmap_malloc, CLOCK_BOOTTIME);

	return bpf_timer_test();
}

 
static int timer_cb3(void *map, int *key, struct bpf_timer *timer)
{
	abs_data += 6;

	if (abs_data < 12) {
		bpf_timer_start(timer, bpf_ktime_get_boot_ns() + 1000,
				BPF_F_TIMER_ABS);
	} else {
		 
		bpf_timer_start(timer, bpf_ktime_get_boot_ns() + (1ull << 35),
				BPF_F_TIMER_ABS);
	}

	return 0;
}

SEC("fentry/bpf_fentry_test3")
int BPF_PROG2(test3, int, a)
{
	int key = 0;
	struct bpf_timer *timer;

	bpf_printk("test3");

	timer = bpf_map_lookup_elem(&abs_timer, &key);
	if (timer) {
		if (bpf_timer_init(timer, &abs_timer, CLOCK_BOOTTIME) != 0)
			err |= 2048;
		bpf_timer_set_callback(timer, timer_cb3);
		bpf_timer_start(timer, bpf_ktime_get_boot_ns() + 1000,
				BPF_F_TIMER_ABS);
	}

	return 0;
}
