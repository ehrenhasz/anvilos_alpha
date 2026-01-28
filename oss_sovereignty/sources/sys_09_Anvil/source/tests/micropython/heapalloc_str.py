import micropython
micropython.heap_lock()
b"" + b""
b"" + b"1"
b"2" + b""
"" + ""
"" + "1"
"2" + ""
"foo".replace(",", "_")
micropython.heap_unlock()
