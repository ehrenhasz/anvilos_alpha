import _thread
def foo():
    foo()
def thread_entry():
    try:
        foo()
    except RuntimeError:
        print("RuntimeError")
    global finished
    finished = True
finished = False
_thread.start_new_thread(thread_entry, ())
while not finished:
    pass
print("done")
