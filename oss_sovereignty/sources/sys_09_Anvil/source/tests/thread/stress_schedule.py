import _thread
import time
import micropython
import gc
try:
    micropython.schedule
except AttributeError:
    print("SKIP")
    raise SystemExit
gc.disable()
_NUM_TASKS = 10000
_TIMEOUT_MS = 10000
n = 0  # How many times the task successfully ran.
t = None  # Start time of test, assigned here to preallocate entry in globals dict.
thread_run = True  # If the thread should continue running.
def task(x):
    global n
    n += 1
def thread():
    while thread_run:
        try:
            micropython.schedule(task, None)
        except RuntimeError:
            time.sleep_ms(10)
for i in range(8):
    try:
        _thread.start_new_thread(thread, ())
    except OSError:
        break
t = time.ticks_ms()
while n < _NUM_TASKS and time.ticks_diff(time.ticks_ms(), t) < _TIMEOUT_MS:
    pass
thread_run = False
time.sleep_ms(20)
if n < _NUM_TASKS:
    print(n)
else:
    print("PASS")
