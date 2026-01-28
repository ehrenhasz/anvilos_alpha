try:
    import asyncio
except ImportError:
    print("SKIP")
    raise SystemExit
async def foo():
    return 42
async def main():
    print(await foo())
    task = asyncio.create_task(foo())
    print(await task)
asyncio.run(main())
