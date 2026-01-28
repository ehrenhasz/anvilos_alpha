import _thread
def foo(i):
    pass
def thread_entry(n, tup):
    for i in tup:
        foo(i)
    with lock:
        global n_finished
        n_finished += 1
lock = _thread.allocate_lock()
n_thread = 0
n_thread_max = 2
n_finished = 0
tup = (1, 2, 3, 4)
for _ in range(n_thread_max):
    try:
        _thread.start_new_thread(thread_entry, (100, tup))
        n_thread += 1
    except OSError:
        break
thread_entry(100, tup)
n_thread += 1
while n_finished < n_thread:
    pass
print(tup)
