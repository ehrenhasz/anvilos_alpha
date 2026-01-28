from . import core
class Lock:
    def __init__(self):
        self.state = 0
        self.waiting = core.TaskQueue()
    def locked(self):
        return self.state == 1
    def release(self):
        if self.state != 1:
            raise RuntimeError("Lock not acquired")
        if self.waiting.peek():
            self.state = self.waiting.pop()
            core._task_queue.push(self.state)
        else:
            self.state = 0
    def acquire(self):
        if self.state != 0:
            self.waiting.push(core.cur_task)
            core.cur_task.data = self.waiting
            try:
                yield
            except core.CancelledError as er:
                if self.state == core.cur_task:
                    self.state = 1
                    self.release()
                raise er
        self.state = 1
        return True
    async def __aenter__(self):
        return await self.acquire()
    async def __aexit__(self, exc_type, exc, tb):
        return self.release()
