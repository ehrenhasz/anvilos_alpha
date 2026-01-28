from . import core
class Event:
    def __init__(self):
        self.state = False  # False=unset; True=set
        self.waiting = core.TaskQueue()  # Queue of Tasks waiting on completion of this event
    def is_set(self):
        return self.state
    def set(self):
        while self.waiting.peek():
            core._task_queue.push(self.waiting.pop())
        self.state = True
    def clear(self):
        self.state = False
    def wait(self):
        if not self.state:
            self.waiting.push(core.cur_task)
            core.cur_task.data = self.waiting
            yield
        return True
try:
    import io
    class ThreadSafeFlag(io.IOBase):
        def __init__(self):
            self.state = 0
        def ioctl(self, req, flags):
            if req == 3:  # MP_STREAM_POLL
                return self.state * flags
            return -1  # Other requests are unsupported
        def set(self):
            self.state = 1
        def clear(self):
            self.state = 0
        async def wait(self):
            if not self.state:
                yield core._io_queue.queue_read(self)
            self.state = 0
except ImportError:
    pass
