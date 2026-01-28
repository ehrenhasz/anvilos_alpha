try:
    import socket, ssl
except ImportError:
    print("SKIP")
    raise SystemExit
ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
print("SSLContext" in str(ctx))
if hasattr(ctx, "__del__"):
    ctx.__del__()
    ctx.__del__()
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.wrap_socket(socket.socket(), do_handshake_on_connect=False)
ctx.wrap_socket(socket.socket(), do_handshake_on_connect=False)
