from .core import *
__version__ = (3, 0, 0)
_attrs = {
    "wait_for": "funcs",
    "wait_for_ms": "funcs",
    "gather": "funcs",
    "Event": "event",
    "ThreadSafeFlag": "event",
    "Lock": "lock",
    "open_connection": "stream",
    "start_server": "stream",
    "StreamReader": "stream",
    "StreamWriter": "stream",
}
def __getattr__(attr):
    mod = _attrs.get(attr, None)
    if mod is None:
        raise AttributeError(attr)
    value = getattr(__import__(mod, None, None, True, 1), attr)
    globals()[attr] = value
    return value
