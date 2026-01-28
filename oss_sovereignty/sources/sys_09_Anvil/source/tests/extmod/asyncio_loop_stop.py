try:
    import asyncio
except ImportError:
    print("SKIP")
    raise SystemExit
async def task():
    print("task")
async def main():
    print("start")
    loop.stop()
    loop.stop()
    print("sleep")
    await asyncio.sleep(0)
    loop.stop()
    asyncio.create_task(task())
    await asyncio.sleep(0)
    print("end")
    loop.stop()
loop = asyncio.new_event_loop()
loop.create_task(main())
for i in range(3):
    print("run", i)
    loop.run_forever()
