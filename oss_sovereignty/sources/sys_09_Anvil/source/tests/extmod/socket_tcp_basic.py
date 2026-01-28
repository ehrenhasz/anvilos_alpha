try:
    import socket, errno
except ImportError:
    print("SKIP")
    raise SystemExit
s = socket.socket()
try:
    s.recv(1)
except OSError as er:
    print("ENOTCONN:", er.errno == errno.ENOTCONN)
