
 

#ifdef DEBUG
bool __init wg_packet_counter_selftest(void)
{
	struct noise_replay_counter *counter;
	unsigned int test_num = 0, i;
	bool success = true;

	counter = kmalloc(sizeof(*counter), GFP_KERNEL);
	if (unlikely(!counter)) {
		pr_err("nonce counter self-test malloc: FAIL\n");
		return false;
	}

#define T_INIT do {                                    \
		memset(counter, 0, sizeof(*counter));  \
		spin_lock_init(&counter->lock);        \
	} while (0)
#define T_LIM (COUNTER_WINDOW_SIZE + 1)
#define T(n, v) do {                                                  \
		++test_num;                                           \
		if (counter_validate(counter, n) != (v)) {            \
			pr_err("nonce counter self-test %u: FAIL\n",  \
			       test_num);                             \
			success = false;                              \
		}                                                     \
	} while (0)

	T_INIT;
	  T(0, true);
	  T(1, true);
	  T(1, false);
	  T(9, true);
	  T(8, true);
	  T(7, true);
	  T(7, false);
	  T(T_LIM, true);
	  T(T_LIM - 1, true);
	  T(T_LIM - 1, false);
	  T(T_LIM - 2, true);
	  T(2, true);
	  T(2, false);
	  T(T_LIM + 16, true);
	  T(3, false);
	  T(T_LIM + 16, false);
	  T(T_LIM * 4, true);
	  T(T_LIM * 4 - (T_LIM - 1), true);
	  T(10, false);
	  T(T_LIM * 4 - T_LIM, false);
	  T(T_LIM * 4 - (T_LIM + 1), false);
	  T(T_LIM * 4 - (T_LIM - 2), true);
	  T(T_LIM * 4 + 1 - T_LIM, false);
	  T(0, false);
	  T(REJECT_AFTER_MESSAGES, false);
	  T(REJECT_AFTER_MESSAGES - 1, true);
	  T(REJECT_AFTER_MESSAGES, false);
	  T(REJECT_AFTER_MESSAGES - 1, false);
	  T(REJECT_AFTER_MESSAGES - 2, true);
	  T(REJECT_AFTER_MESSAGES + 1, false);
	  T(REJECT_AFTER_MESSAGES + 2, false);
	  T(REJECT_AFTER_MESSAGES - 2, false);
	  T(REJECT_AFTER_MESSAGES - 3, true);
	  T(0, false);

	T_INIT;
	for (i = 1; i <= COUNTER_WINDOW_SIZE; ++i)
		T(i, true);
	T(0, true);
	T(0, false);

	T_INIT;
	for (i = 2; i <= COUNTER_WINDOW_SIZE + 1; ++i)
		T(i, true);
	T(1, true);
	T(0, false);

	T_INIT;
	for (i = COUNTER_WINDOW_SIZE + 1; i-- > 0;)
		T(i, true);

	T_INIT;
	for (i = COUNTER_WINDOW_SIZE + 2; i-- > 1;)
		T(i, true);
	T(0, false);

	T_INIT;
	for (i = COUNTER_WINDOW_SIZE + 1; i-- > 1;)
		T(i, true);
	T(COUNTER_WINDOW_SIZE + 1, true);
	T(0, false);

	T_INIT;
	for (i = COUNTER_WINDOW_SIZE + 1; i-- > 1;)
		T(i, true);
	T(0, true);
	T(COUNTER_WINDOW_SIZE + 1, true);

#undef T
#undef T_LIM
#undef T_INIT

	if (success)
		pr_info("nonce counter self-tests: pass\n");
	kfree(counter);
	return success;
}
#endif
