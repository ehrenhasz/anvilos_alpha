def __getattr__(attr):
    import asyncio
    return getattr(asyncio, attr)
