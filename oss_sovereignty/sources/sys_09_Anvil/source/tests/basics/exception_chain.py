try:
    raise Exception from None
except Exception:
    print("Caught Exception")
try:
    try:
        raise ValueError("Value")
    except Exception as exc:
        raise RuntimeError("Runtime") from exc
except Exception as ex2:
    print("Caught Exception:", ex2)
