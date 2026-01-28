import _thread
tid = None
tid_main = None
new_tid = None
finished = False
def thread_entry():
    global tid
    tid = _thread.get_ident()
    print("thread", type(tid) == int, tid != 0, tid != tid_main)
    global finished
    finished = True
tid_main = _thread.get_ident()
print("main", type(tid_main) == int, tid_main != 0)
new_tid = _thread.start_new_thread(thread_entry, ())
while not finished:
    pass
print("done", type(new_tid) == int, new_tid == tid)
