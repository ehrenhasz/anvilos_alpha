import _thread
def foo():
    raise ValueError
def thread_entry():
    try:
        foo()
    except ValueError:
        pass
    with lock:
        global n_finished
        n_finished += 1
lock = _thread.allocate_lock()
n_thread = 4
n_finished = 0
for i in range(n_thread):
    while True:
        try:
            _thread.start_new_thread(thread_entry, ())
            break
        except OSError:
            pass
while n_finished < n_thread:
    pass
print("done")
