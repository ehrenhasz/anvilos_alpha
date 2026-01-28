from __future__ import print_function
from test_channel import ThreadedTCPServer, TestingRequestHandler, \
    writePortInFile
import socket
import threading
import os
try:
    FileNotFoundError
except NameError:
    FileNotFoundError = (IOError, OSError)
if not hasattr(socket, "AF_UNIX"):
    raise NotImplementedError("Unix sockets are not supported on this platform")
class ThreadedUnixServer(ThreadedTCPServer):
    address_family = socket.AF_UNIX
class ThreadedUnixRequestHandler(TestingRequestHandler):
    pass
def main(path):
    server = ThreadedUnixServer(path, ThreadedUnixRequestHandler)
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.start()
    writePortInFile(1234)
    print("Listening on {0}".format(server.server_address))
    try:
        while server_thread.is_alive():
            server_thread.join(1)
    except (KeyboardInterrupt, SystemExit):
        server.shutdown()
if __name__ == "__main__":
    try:
        os.remove("Xtestsocket")
    except FileNotFoundError:
        pass
    main("Xtestsocket")
