try:
    import asyncio
except ImportError:
    print("SKIP")
    raise SystemExit
async def main():
    print("main start")
    raise ValueError(1)
    print("main done")
try:
    asyncio.run(main())
except ValueError as er:
    print("ValueError", er.args[0])
async def task():
    print("task start")
    raise ValueError(2)
    print("task done")
async def main():
    print("main start")
    t = asyncio.create_task(task())
    await t
    print("main done")
try:
    asyncio.run(main())
except ValueError as er:
    print("ValueError", er.args[0])
async def task():
    pass
async def main():
    print("main start")
    asyncio.create_task(task())
    raise ValueError(3)
    print("main done")
try:
    asyncio.run(main())
except ValueError as er:
    print("ValueError", er.args[0])
