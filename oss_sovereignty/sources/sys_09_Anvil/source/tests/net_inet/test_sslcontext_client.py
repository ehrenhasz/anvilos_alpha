import os
import socket
import ssl
ca_cert_chain = "mpycert.der"
try:
    os.stat(ca_cert_chain)
except OSError:
    print("SKIP")
    raise SystemExit
def main(use_stream=True):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.verify_mode = ssl.CERT_REQUIRED
    assert context.verify_mode == ssl.CERT_REQUIRED
    context.load_verify_locations(cafile=ca_cert_chain)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    addr = socket.getaddrinfo("micropython.org", 443)[0][-1]
    s.connect(addr)
    ssl_sock = context.wrap_socket(s, server_hostname="micropython.org")
    ssl_sock.write(b"GET / HTTP/1.0\r\n\r\n")
    print(ssl_sock.read(17))
    assert isinstance(ssl_sock.cipher(), tuple)
    ssl_sock.close()
main()
