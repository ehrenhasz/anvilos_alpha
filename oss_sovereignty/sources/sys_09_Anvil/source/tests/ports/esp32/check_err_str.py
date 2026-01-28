try:
    from esp32 import Partition as p
    import micropython
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    open("this filedoesnotexist", "r")
    print("FAILED TO RAISE")
except OSError as e:
    print(e)
part = p.find(type=p.TYPE_DATA)[0]
fun = p.set_boot
try:
    fun(part)
    print("FAILED TO RAISE")
except OSError as e:
    print(e)
exc = "FAILED TO RAISE"
micropython.heap_lock()
try:
    fun(part)
except OSError as e:
    exc = e
micropython.heap_unlock()
print("exc:", exc)  
micropython.alloc_emergency_exception_buf(256)
exc = "FAILED TO RAISE"
micropython.heap_lock()
try:
    fun(part)
except Exception as e:
    exc = e
micropython.heap_unlock()
print(exc)
