try:
    import asyncio
except ImportError:
    print("SKIP")
    raise SystemExit
async def task(s, allow_cancel):
    try:
        print("task start")
        await asyncio.sleep(s)
        print("task done")
    except asyncio.CancelledError as er:
        print("task cancel")
        if allow_cancel:
            raise er
async def task2(allow_cancel):
    print("task 2")
    try:
        await asyncio.create_task(task(0.05, allow_cancel))
    except asyncio.CancelledError as er:
        print("task 2 cancel")
        raise er
    print("task 2 done")
async def main():
    t = asyncio.create_task(task(2, True))
    print(t.cancel())
    t = asyncio.create_task(task(2, True))
    await asyncio.sleep(0.01)
    print(t.cancel())
    print("main sleep")
    await asyncio.sleep(0.01)
    t = asyncio.create_task(task(2, True))
    await asyncio.sleep(0.01)
    for _ in range(4):
        print(t.cancel())
    print("main sleep")
    await asyncio.sleep(0.01)
    print("main wait")
    try:
        await t
    except asyncio.CancelledError:
        print("main got CancelledError")
    t = asyncio.create_task(task(0.01, False))
    await asyncio.sleep(0.05)
    print(t.cancel())
    print("----")
    t = asyncio.create_task(task2(True))
    await asyncio.sleep(0.01)
    print("main cancel")
    t.cancel()
    print("main sleep")
    await asyncio.sleep(0.1)
    print("----")
    t = asyncio.create_task(task2(False))
    await asyncio.sleep(0.01)
    print("main cancel")
    t.cancel()
    print("main sleep")
    await asyncio.sleep(0.1)
asyncio.run(main())
