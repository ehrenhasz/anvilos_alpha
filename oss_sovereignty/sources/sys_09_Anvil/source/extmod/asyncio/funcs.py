from . import core
async def _run(waiter, aw):
    try:
        result = await aw
        status = True
    except BaseException as er:
        result = None
        status = er
    if waiter.data is None:
        if waiter.cancel():
            waiter.data = core.CancelledError(status, result)
async def wait_for(aw, timeout, sleep=core.sleep):
    aw = core._promote_to_task(aw)
    if timeout is None:
        return await aw
    runner_task = core.create_task(_run(core.cur_task, aw))
    try:
        await sleep(timeout)
    except core.CancelledError as er:
        status = er.value
        if status is None:
            runner_task.cancel()
            raise er
        elif status is True:
            return er.args[1]
        else:
            raise status
    runner_task.cancel()
    await runner_task
    raise core.TimeoutError
def wait_for_ms(aw, timeout):
    return wait_for(aw, timeout, core.sleep_ms)
class _Remove:
    @staticmethod
    def remove(t):
        pass
def gather(*aws, return_exceptions=False):
    def done(t, er):
        nonlocal state
        if gather_task.data is not _Remove:
            return
        elif not return_exceptions and not isinstance(er, StopIteration):
            state = er
        else:
            state -= 1
            if state:
                return
        core._task_queue.push(gather_task)
    ts = [core._promote_to_task(aw) for aw in aws]
    state = 0
    for i in range(len(ts)):
        if ts[i].state is True:
            ts[i].state = done
            state += 1
        elif not ts[i].state:
            if not isinstance(ts[i].data, StopIteration):
                if not return_exceptions:
                    state = -len(ts)
        else:
            raise RuntimeError("can't gather")
    gather_task = core.cur_task
    cancel_all = False
    if state > 0:
        gather_task.data = _Remove
        try:
            yield
        except core.CancelledError as er:
            cancel_all = True
            state = er
    for i in range(len(ts)):
        if ts[i].state is done:
            ts[i].state = True
            if cancel_all:
                ts[i].cancel()
        elif isinstance(ts[i].data, StopIteration):
            ts[i] = ts[i].data.value
        else:
            if return_exceptions:
                ts[i] = ts[i].data
            elif isinstance(state, int):
                state = ts[i].data
    if state:
        raise state
    return ts
