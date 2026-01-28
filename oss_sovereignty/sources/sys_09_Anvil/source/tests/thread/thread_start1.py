import time
import _thread
def foo():
    pass
def thread_entry(n):
    for i in range(n):
        foo()
for i in range(2):
    while True:
        try:
            _thread.start_new_thread(thread_entry, ((i + 1) * 10,))
            break
        except OSError:
            pass
time.sleep(1)
print("done")
