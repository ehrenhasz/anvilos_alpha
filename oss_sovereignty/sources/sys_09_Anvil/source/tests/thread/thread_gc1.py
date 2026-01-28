import gc
import _thread
def thread_entry(n):
    data = bytearray(i for i in range(256))
    for i in range(n):
        for i in range(len(data)):
            data[i] = data[i]
        gc.collect()
    with lock:
        global n_correct, n_finished
        n_correct += list(data) == list(range(256))
        n_finished += 1
lock = _thread.allocate_lock()
n_thread = 0
n_thread_max = 4
n_correct = 0
n_finished = 0
for _ in range(n_thread_max):
    try:
        _thread.start_new_thread(thread_entry, (10,))
        n_thread += 1
    except OSError:
        break
thread_entry(10)
n_thread += 1
while n_finished < n_thread:
    pass
print(n_correct == n_finished)
