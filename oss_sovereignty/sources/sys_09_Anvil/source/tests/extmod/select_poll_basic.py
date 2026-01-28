try:
    import socket, select, errno
    select.poll  
except (ImportError, AttributeError):
    print("SKIP")
    raise SystemExit
poller = select.poll()
s = socket.socket()
poller.register(s)
poller.register(s)
poller.register(s, select.POLLIN | select.POLLOUT)
try:
    poller.modify(s)
except TypeError:
    print("modify:TypeError")
poller.modify(s, select.POLLIN)
poller.unregister(s)
try:
    poller.modify(s, select.POLLIN)
except OSError as e:
    assert e.errno == errno.ENOENT
poller.register(s)
s.close()
p = poller.poll(0)
print(len(p), p[0][-1])
