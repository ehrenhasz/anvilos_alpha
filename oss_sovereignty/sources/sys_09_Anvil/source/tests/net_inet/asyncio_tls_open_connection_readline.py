import ssl
import os
import asyncio
ca_cert_chain = "isrg.der"
try:
    os.stat(ca_cert_chain)
except OSError:
    print("SKIP")
    raise SystemExit
with open(ca_cert_chain, "rb") as ca:
    cadata = ca.read()
client_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
client_ctx.verify_mode = ssl.CERT_REQUIRED
client_ctx.load_verify_locations(cadata=cadata)
async def http_get(url, port, sslctx):
    reader, writer = await asyncio.open_connection(url, port, ssl=sslctx)
    print("write GET")
    writer.write(b"GET / HTTP/1.0\r\n\r\n")
    await writer.drain()
    print("read response")
    while True:
        data = await reader.readline()
        if b"GMT" not in data:
            print("read:", data)
        if not data:
            break
    print("close")
    writer.close()
    await writer.wait_closed()
    print("done")
asyncio.run(http_get("micropython.org", 443, client_ctx))
