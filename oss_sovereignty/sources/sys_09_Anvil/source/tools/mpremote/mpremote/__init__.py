try:
    from importlib.metadata import version, PackageNotFoundError
    try:
        __version__ = version("mpremote")
    except PackageNotFoundError:
        __version__ = "0.0.0-local"
except ImportError:
    __version__ = "0.0.0-unknown"
