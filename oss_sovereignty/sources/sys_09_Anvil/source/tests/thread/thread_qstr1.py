import time
import _thread
def check(s, val):
    assert type(s) == str
    assert int(s) == val
def th(base, n):
    for i in range(n):
        exec("check('%u', %u)" % (base + i, base + i))
    with lock:
        global n_finished
        n_finished += 1
lock = _thread.allocate_lock()
n_thread = 0
n_thread_max = 4
n_finished = 0
n_qstr_per_thread = 100  
for _ in range(n_thread_max):
    try:
        _thread.start_new_thread(th, (n_thread * n_qstr_per_thread, n_qstr_per_thread))
        n_thread += 1
    except OSError:
        break
th(n_thread * n_qstr_per_thread, n_qstr_per_thread)
n_thread += 1
while n_finished < n_thread:
    time.sleep(0)
print("pass")
