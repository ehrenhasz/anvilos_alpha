from __future__ import print_function
import json
import socket
import sys
import threading
try:
    import socketserver
except ImportError:
    import SocketServer as socketserver
thesocket = None
class ThreadedTCPRequestHandler(socketserver.BaseRequestHandler):
    def handle(self):
        print("=== socket opened ===")
        global thesocket
        thesocket = self.request
        while True:
            try:
                data = self.request.recv(4096).decode('utf-8')
            except socket.error:
                print("=== socket error ===")
                break
            if data == '':
                print("=== socket closed ===")
                break
            print("received: {0}".format(data))
            try:
                decoded = json.loads(data)
            except ValueError:
                print("json decoding failed")
                decoded = [-1, '']
            if decoded[0] >= 0:
                if decoded[1] == 'hello!':
                    response = "got it"
                    id = decoded[0]
                elif decoded[1] == 'hello channel!':
                    response = "got that"
                    id = 0
                else:
                    response = "what?"
                    id = decoded[0]
                encoded = json.dumps([id, response])
                print("sending {0}".format(encoded))
                self.request.sendall(encoded.encode('utf-8'))
        thesocket = None
class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    pass
if __name__ == "__main__":
    HOST, PORT = "localhost", 8765
    server = ThreadedTCPServer((HOST, PORT), ThreadedTCPRequestHandler)
    ip, port = server.server_address
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.daemon = True
    server_thread.start()
    print("Server loop running in thread: ", server_thread.name)
    print("Listening on port {0}".format(PORT))
    while True:
        typed = sys.stdin.readline()
        if "quit" in typed:
            print("Goodbye!")
            break
        if thesocket is None:
            print("No socket yet")
        else:
            print("sending {0}".format(typed))
            thesocket.sendall(typed.encode('utf-8'))
    server.shutdown()
    server.server_close()
