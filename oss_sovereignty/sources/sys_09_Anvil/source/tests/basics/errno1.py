try:
    import errno
except ImportError:
    print("SKIP")
    raise SystemExit
print(type(errno.EIO))
msg = str(OSError(errno.EIO))
print(msg[:7], msg[-5:])
msg = str(OSError(errno.EIO, "details"))
print(msg[:7], msg[-14:])
msg = str(OSError(errno.EIO, "details", "more details"))
print(msg[:1], msg[-28:])
print(str(OSError(9999)))
errno = errno
print(errno.__name__)
