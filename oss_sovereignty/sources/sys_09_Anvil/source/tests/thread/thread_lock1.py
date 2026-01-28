import _thread
lock = _thread.allocate_lock()
print(type(lock) == _thread.LockType)
print(lock.locked())
print(lock.acquire())
print(lock.locked())
lock.release()
print(lock.locked())
print(lock.acquire())
print(lock.locked())
print(lock.acquire(0))
print(lock.locked())
lock.release()
print(lock.locked())
with lock:
    print(lock.locked())
try:
    with lock:
        print(lock.locked())
        raise KeyError
except KeyError:
    print("KeyError")
    print(lock.locked())
try:
    lock.release()
except RuntimeError:
    print("RuntimeError")
