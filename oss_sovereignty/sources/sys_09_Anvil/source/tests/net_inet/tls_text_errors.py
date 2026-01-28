import socket, ssl
def test(addr):
    s = socket.socket()
    s.connect(addr)
    try:
        ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        if hasattr(ssl_context, "check_hostname"):
            ssl_context.check_hostname = False
        s = ssl_context.wrap_socket(s)
        print("wrap: no exception")
    except OSError as e:
        ok = (
            "SSL_INVALID_RECORD" in str(e)
            or "RECORD_OVERFLOW" in str(e)
            or "wrong version" in str(e)
            or "record layer failure" in str(e)
        )
        print("wrap:", ok)
        if not ok:
            print("got exception:", e)
    s.close()
if __name__ == "__main__":
    addr = socket.getaddrinfo("micropython.org", 80)[0][-1]
    test(addr)
