try:
    import asyncio
except ImportError:
    print("SKIP")
    raise SystemExit
async def task():
    for i in range(4):
        print("task", i)
        await asyncio.sleep(0)
        await asyncio.sleep(0)
async def main():
    print("start")
    loop.create_task(task())
    await asyncio.sleep(0)
    print("stop")
    loop.stop()
loop = asyncio.get_event_loop()
loop.create_task(main())
loop.run_forever()
loop = asyncio.new_event_loop()
loop.create_task(main())
loop.run_forever()
