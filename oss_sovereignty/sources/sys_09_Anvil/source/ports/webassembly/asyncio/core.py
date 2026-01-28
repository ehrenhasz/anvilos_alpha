from time import ticks_ms as ticks, ticks_diff, ticks_add
import sys, js, jsffi
from _asyncio import TaskQueue, Task
class CancelledError(BaseException):
    pass
class TimeoutError(Exception):
    pass
_exc_context = {"message": "Task exception wasn't retrieved", "exception": None, "future": None}
class SingletonGenerator:
    def __init__(self):
        self.state = None
        self.exc = StopIteration()
    def __iter__(self):
        return self
    def __next__(self):
        if self.state is not None:
            _task_queue.push(cur_task, self.state)
            self.state = None
            return None
        else:
            self.exc.__traceback__ = None
            raise self.exc
def sleep_ms(t, sgen=SingletonGenerator()):
    if cur_task is None:
        return jsffi.async_timeout_ms(t)
    assert sgen.state is None
    sgen.state = ticks_add(ticks(), max(0, t))
    return sgen
def sleep(t):
    return sleep_ms(int(t * 1000))
asyncio_timer = None
class ThenableEvent:
    def __init__(self, thenable):
        self.result = None  
        self.waiting = None  
        thenable.then(self.set)
    def set(self, value=None):
        self.result = value
        if self.waiting:
            _task_queue.push(self.waiting)
            self.waiting = None
            _schedule_run_iter(0)
    def remove(self, task):
        self.waiting = None
    def wait(self):
        self.waiting = cur_task
        cur_task.data = self
        yield
        return self.result
def _promote_to_task(aw):
    return aw if isinstance(aw, Task) else create_task(aw)
def _schedule_run_iter(dt):
    global asyncio_timer
    if asyncio_timer is not None:
        js.clearTimeout(asyncio_timer)
    asyncio_timer = js.setTimeout(_run_iter, dt)
def _run_iter():
    global cur_task
    excs_all = (CancelledError, Exception)  
    excs_stop = (CancelledError, StopIteration)  
    while True:
        t = _task_queue.peek()
        if t:
            dt = max(0, ticks_diff(t.ph_key, ticks()))
        else:
            cur_task = None
            return
        if dt > 0:
            cur_task = None
            _schedule_run_iter(dt)
            return
        t = _task_queue.pop()
        cur_task = t
        try:
            exc = t.data
            if not exc:
                t.coro.send(None)
            else:
                t.data = None
                t.coro.throw(exc)
        except excs_all as er:
            assert t.data is None
            if t.state:
                waiting = False
                if t.state is True:
                    t.state = None
                elif callable(t.state):
                    t.state(t, er)
                    t.state = False
                    waiting = True
                else:
                    while t.state.peek():
                        _task_queue.push(t.state.pop())
                        waiting = True
                    t.state = False
                if not waiting and not isinstance(er, excs_stop):
                    _task_queue.push(t)
                t.data = er
            elif t.state is None:
                t.data = exc
                _exc_context["exception"] = exc
                _exc_context["future"] = t
                Loop.call_exception_handler(_exc_context)
def create_task(coro):
    if not hasattr(coro, "send"):
        raise TypeError("coroutine expected")
    t = Task(coro, globals())
    _task_queue.push(t)
    _schedule_run_iter(0)
    return t
cur_task = None
class Loop:
    _exc_handler = None
    def create_task(coro):
        return create_task(coro)
    def close():
        pass
    def set_exception_handler(handler):
        Loop._exc_handler = handler
    def get_exception_handler():
        return Loop._exc_handler
    def default_exception_handler(loop, context):
        print(context["message"], file=sys.stderr)
        print("future:", context["future"], "coro=", context["future"].coro, file=sys.stderr)
        sys.print_exception(context["exception"], sys.stderr)
    def call_exception_handler(context):
        (Loop._exc_handler or Loop.default_exception_handler)(Loop, context)
def get_event_loop():
    return Loop
def current_task():
    if cur_task is None:
        raise RuntimeError("no running event loop")
    return cur_task
def new_event_loop():
    global _task_queue
    _task_queue = TaskQueue()  
    return Loop
new_event_loop()
