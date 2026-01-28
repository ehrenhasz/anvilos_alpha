try:
    import tls
except ImportError:
    print("SKIP")
    raise SystemExit
try:
    tls.SSLContext()
except TypeError:
    print("TypeError")
ctx = tls.SSLContext(tls.PROTOCOL_TLS_CLIENT)
try:
    ctx.does_not_exist
except AttributeError:
    print("AttributeError on load")
try:
    ctx.does_not_exist = None
except AttributeError:
    print("AttributeError on store")
try:
    del ctx.does_not_exist
except AttributeError:
    print("AttributeError on delete")
