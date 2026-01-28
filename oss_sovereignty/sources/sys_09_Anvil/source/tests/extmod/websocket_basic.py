try:
    import io
    import errno
    import websocket
except ImportError:
    print("SKIP")
    raise SystemExit
def ws_read(msg, sz):
    ws = websocket.websocket(io.BytesIO(msg))
    return ws.read(sz)
def ws_write(msg, sz):
    s = io.BytesIO()
    ws = websocket.websocket(s)
    ws.write(msg)
    s.seek(0)
    return s.read(sz)
print(ws_read(b"\x81\x04ping", 4))
print(ws_read(b"\x80\x04ping", 4))  # FRAME_CONT
print(ws_write(b"pong", 6))
print(ws_read(b"\x81~\x00\x80" + b"ping" * 32, 128))
print(ws_write(b"pong" * 32, 132))
print(ws_read(b"\x81\x84maskmask", 4))
s = io.BytesIO(b"\x88\x00")  # FRAME_CLOSE
ws = websocket.websocket(s)
print(ws.read(1))
s.seek(2)
print(s.read(4))
print(ws_read(b"\x89\x00\x81\x04ping", 4))  # FRAME_PING
print(ws_read(b"\x8a\x00\x81\x04pong", 4))  # FRAME_PONG
ws = websocket.websocket(io.BytesIO())
ws.close()
ws = websocket.websocket(io.BytesIO())
print(ws.ioctl(8))  # GET_DATA_OPTS
print(ws.ioctl(9, 2))  # SET_DATA_OPTS
print(ws.ioctl(9))
try:
    ws.ioctl(-1)
except OSError as e:
    print("ioctl: EINVAL:", e.errno == errno.EINVAL)
