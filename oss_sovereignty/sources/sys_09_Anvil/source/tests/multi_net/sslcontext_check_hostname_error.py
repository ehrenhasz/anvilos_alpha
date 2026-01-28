try:
    import os
    import socket
    import ssl
except ImportError:
    print("SKIP")
    raise SystemExit
PORT = 8000
cert = cafile = "ec_cert.der"
key = "ec_key.der"
try:
    os.stat(cafile)
    os.stat(key)
except OSError:
    print("SKIP")
    raise SystemExit
def instance0():
    multitest.globals(IP=multitest.get_network_ip())
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(socket.getaddrinfo("0.0.0.0", PORT)[0][-1])
    s.listen(1)
    multitest.next()
    s2, _ = s.accept()
    server_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    server_ctx.load_cert_chain(cert, key)
    multitest.broadcast("ready")
    try:
        s2 = server_ctx.wrap_socket(s2, server_side=True)
    except Exception as e:
        print(e)
    s.close()
def instance1():
    multitest.next()
    s = socket.socket()
    s.connect(socket.getaddrinfo(IP, PORT)[0][-1])
    client_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    client_ctx.verify_mode = ssl.CERT_REQUIRED
    client_ctx.load_verify_locations(cafile=cafile)
    multitest.wait("ready")
    try:
        s = client_ctx.wrap_socket(s)
    except Exception as e:
        print(e)
    s.close()
