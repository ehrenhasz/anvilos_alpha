import micropython
if not hasattr(micropython, "mem_info"):
    print("SKIP")
else:
    micropython.mem_info()
    micropython.mem_info(1)
    micropython.qstr_info()
    micropython.qstr_info(1)
