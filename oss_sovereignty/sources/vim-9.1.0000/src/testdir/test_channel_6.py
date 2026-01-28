from test_channel import main, ThreadedTCPServer
import socket
class ThreadedTCP6Server(ThreadedTCPServer):
    address_family = socket.AF_INET6
if __name__ == "__main__":
    main("::", 0, ThreadedTCP6Server)
