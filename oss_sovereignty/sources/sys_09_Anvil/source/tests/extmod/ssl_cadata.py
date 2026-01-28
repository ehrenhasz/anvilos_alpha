try:
    import io
    import ssl
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    ssl.wrap_socket(io.BytesIO(), cadata=b"!")
except TypeError:
    print("SKIP")
    raise SystemExit
except ValueError as er:
    print(repr(er))
