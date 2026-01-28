package(
    "asyncio",
    (
        "__init__.py",
        "core.py",
        "event.py",
        "funcs.py",
        "lock.py",
        "stream.py",
    ),
    base_path="..",
    opt=3,
)
module("uasyncio.py", opt=3)
