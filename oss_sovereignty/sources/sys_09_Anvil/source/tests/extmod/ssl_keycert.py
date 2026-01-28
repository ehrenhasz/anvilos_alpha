try:
    import io
    import ssl
except ImportError:
    print("SKIP")
    raise SystemExit
key = b"0\x82\x019\x02\x01\x00\x02A\x00\xf9\xe0}\xbd\xd7\x9cI\x18\x06\xc3\xcb\xb5\xec@r\xfbD\x18\x80\xaaWoZ{\xcc\xa3\xeb!\"\x0fY\x9e]-\xee\xe4\t!BY\x9f{7\xf3\xf2\x8f}}\r|.\xa8<\ta\xb2\xd7W\xb3\xc9\x19A\xc39\x02\x03\x01\x00\x01\x02@\x07:\x9fh\xa6\x9c6\xe1
try:
    ssl.wrap_socket(io.BytesIO(), key=b"!")
except ValueError as er:
    print(repr(er))
try:
    ssl.wrap_socket(io.BytesIO(), key=key)
except TypeError as er:
    print(repr(er))
try:
    ssl.wrap_socket(io.BytesIO(), key=key, cert=b"!")
except ValueError as er:
    print(repr(er))
