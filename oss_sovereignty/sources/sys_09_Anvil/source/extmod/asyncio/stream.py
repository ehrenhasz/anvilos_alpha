from . import core
class Stream:
    def __init__(self, s, e={}):
        self.s = s
        self.e = e
        self.out_buf = b""
    def get_extra_info(self, v):
        return self.e[v]
    def close(self):
        pass
    async def wait_closed(self):
        self.s.close()
    def read(self, n=-1):
        r = b""
        while True:
            yield core._io_queue.queue_read(self.s)
            r2 = self.s.read(n)
            if r2 is not None:
                if n >= 0:
                    return r2
                if not len(r2):
                    return r
                r += r2
    def readinto(self, buf):
        yield core._io_queue.queue_read(self.s)
        return self.s.readinto(buf)
    def readexactly(self, n):
        r = b""
        while n:
            yield core._io_queue.queue_read(self.s)
            r2 = self.s.read(n)
            if r2 is not None:
                if not len(r2):
                    raise EOFError
                r += r2
                n -= len(r2)
        return r
    def readline(self):
        l = b""
        while True:
            yield core._io_queue.queue_read(self.s)
            l2 = self.s.readline()  # may do multiple reads but won't block
            if l2 is None:
                continue
            l += l2
            if not l2 or l[-1] == 10:  # \n (check l in case l2 is str)
                return l
    def write(self, buf):
        if not self.out_buf:
            ret = self.s.write(buf)
            if ret == len(buf):
                return
            if ret is not None:
                buf = buf[ret:]
        self.out_buf += buf
    def drain(self):
        if not self.out_buf:
            return (yield from core.sleep_ms(0))
        mv = memoryview(self.out_buf)
        off = 0
        while off < len(mv):
            yield core._io_queue.queue_write(self.s)
            ret = self.s.write(mv[off:])
            if ret is not None:
                off += ret
        self.out_buf = b""
StreamReader = Stream
StreamWriter = Stream
def open_connection(host, port, ssl=None, server_hostname=None):
    from errno import EINPROGRESS
    import socket
    ai = socket.getaddrinfo(host, port, 0, socket.SOCK_STREAM)[0]  # TODO this is blocking!
    s = socket.socket(ai[0], ai[1], ai[2])
    s.setblocking(False)
    try:
        s.connect(ai[-1])
    except OSError as er:
        if er.errno != EINPROGRESS:
            raise er
    if ssl:
        if ssl is True:
            import ssl as _ssl
            ssl = _ssl.SSLContext(_ssl.PROTOCOL_TLS_CLIENT)
        if not server_hostname:
            server_hostname = host
        s = ssl.wrap_socket(s, server_hostname=server_hostname, do_handshake_on_connect=False)
        s.setblocking(False)
    ss = Stream(s)
    yield core._io_queue.queue_write(s)
    return ss, ss
class Server:
    async def __aenter__(self):
        return self
    async def __aexit__(self, exc_type, exc, tb):
        self.close()
        await self.wait_closed()
    def close(self):
        self.state = True
        self.task.cancel()
    async def wait_closed(self):
        await self.task
    async def _serve(self, s, cb, ssl):
        self.state = False
        while True:
            try:
                yield core._io_queue.queue_read(s)
            except core.CancelledError as er:
                s.close()
                if self.state:
                    return
                else:
                    raise er
            try:
                s2, addr = s.accept()
            except:
                continue
            if ssl:
                try:
                    s2 = ssl.wrap_socket(s2, server_side=True, do_handshake_on_connect=False)
                except OSError as e:
                    core.sys.print_exception(e)
                    s2.close()
                    continue
            s2.setblocking(False)
            s2s = Stream(s2, {"peername": addr})
            core.create_task(cb(s2s, s2s))
async def start_server(cb, host, port, backlog=5, ssl=None):
    import socket
    host = socket.getaddrinfo(host, port)[0]  # TODO this is blocking!
    s = socket.socket()
    s.setblocking(False)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(host[-1])
    s.listen(backlog)
    srv = Server()
    srv.task = core.create_task(srv._serve(s, cb, ssl))
    try:
        await core.sleep_ms(0)
    except core.CancelledError as er:
        srv.task.cancel()
        raise er
    return srv
async def stream_awrite(self, buf, off=0, sz=-1):
    if off != 0 or sz != -1:
        buf = memoryview(buf)
        if sz == -1:
            sz = len(buf)
        buf = buf[off : off + sz]
    self.write(buf)
    await self.drain()
Stream.aclose = Stream.wait_closed
Stream.awrite = stream_awrite
Stream.awritestr = stream_awrite  # TODO explicitly convert to bytes?
