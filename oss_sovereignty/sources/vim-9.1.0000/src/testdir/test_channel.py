from __future__ import print_function
import json
import socket
import sys
import time
import threading
try:
    import socketserver
except ImportError:
    import SocketServer as socketserver
class TestingRequestHandler(socketserver.BaseRequestHandler):
    def handle(self):
        print("=== socket opened ===")
        while True:
            try:
                received = self.request.recv(4096).decode('utf-8')
            except socket.error:
                print("=== socket error ===")
                break
            except IOError:
                print("=== socket closed ===")
                break
            if received == '':
                print("=== socket closed ===")
                break
            print("received: {0}".format(received))
            todo = received
            while todo != '':
                splitidx = todo.find('\n')
                if splitidx < 0:
                     used = todo
                     todo = ''
                else:
                     used = todo[:splitidx]
                     todo = todo[splitidx + 1:]
                if used != received:
                    print("using: {0}".format(used))
                try:
                    decoded = json.loads(used)
                except ValueError:
                    print("json decoding failed")
                    decoded = [-1, '']
                if decoded[0] >= 0:
                    if decoded[1] == 'hello!':
                        response = "got it"
                    elif decoded[1] == 'malformed1':
                        cmd = '["ex",":"]wrong!["ex","smi"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                        time.sleep(0.2)
                    elif decoded[1] == 'malformed2':
                        cmd = '"unterminated string'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                        time.sleep(0.2)
                    elif decoded[1] == 'malformed3':
                        cmd = '["ex","missing ]"'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                        time.sleep(0.2)
                    elif decoded[1] == 'split':
                        cmd = '["ex","let '
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        time.sleep(0.01)
                        cmd = 'g:split = 123"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1].startswith("echo "):
                        response = decoded[1][5:]
                    elif decoded[1] == 'make change':
                        cmd = '["ex","call append(\\"$\\",\\"added1\\")"]'
                        cmd += '["ex","call append(\\"$\\",\\"added2\\")"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'echoerr':
                        cmd = '["ex","echoerr \\\"this is an error\\\""]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                        time.sleep(0.02)
                    elif decoded[1] == 'bad command':
                        cmd = '["ex","foo bar"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'do normal':
                        cmd = '["normal","G$s more\u001b"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'eval-works':
                        cmd = '["expr","\\"foo\\" . 123", -1]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'eval-special':
                        cmd = '["expr","\\"foo\x7f\x10\x01bar\\"", -2]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'eval-getline':
                        cmd = '["expr","getline(3)", -3]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'eval-fails':
                        cmd = '["expr","xxx", -4]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'eval-error':
                        cmd = '["expr","function(\\"tr\\")", -5]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'eval-bad':
                        cmd = '["expr","xxx"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'an expr':
                        cmd = '["expr","setline(\\"$\\", [\\"one\\",\\"two\\",\\"three\\"])"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'call-func':
                        cmd = '["call","MyFunction",[1,2,3], 0]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'redraw':
                        cmd = '["redraw",""]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'redraw!':
                        cmd = '["redraw","force"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'empty-request':
                        cmd = '[]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'eval-result':
                        response = last_eval
                    elif decoded[1] == 'call me':
                        cmd = '[0,"we called you"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "ok"
                    elif decoded[1] == 'call me again':
                        cmd = '[0,"we did call you"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = ""
                    elif decoded[1] == 'send zero':
                        cmd = '[0,"zero index"]'
                        print("sending: {0}".format(cmd))
                        self.request.sendall(cmd.encode('utf-8'))
                        response = "sent zero"
                    elif decoded[1] == 'close me':
                        print("closing")
                        self.request.close()
                        response = ""
                    elif decoded[1] == 'wait a bit':
                        time.sleep(0.2)
                        response = "waited"
                    elif decoded[1] == '!quit!':
                        self.server.shutdown()
                        return
                    elif decoded[1] == '!crash!':
                        42 / 0
                    else:
                        response = "what?"
                    if response == "":
                        print("no response")
                    else:
                        encoded = json.dumps([decoded[0], response])
                        print("sending: {0}".format(encoded))
                        self.request.sendall(encoded.encode('utf-8'))
                elif decoded[0] < 0:
                    last_eval = decoded
class ThreadedTCPRequestHandler(TestingRequestHandler):
    def setup(self):
        self.request.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    pass
def writePortInFile(port):
    f = open("Xportnr", "w")
    f.write("{0}".format(port))
    f.close()
def main(host, port, server_class=ThreadedTCPServer):
    if len(sys.argv) >= 2 and sys.argv[1] == 'delay':
        port = 13684
        writePortInFile(port)
        print("Wait for it...")
        time.sleep(0.5)
    addrs = socket.getaddrinfo(host, port, 0, 0, socket.IPPROTO_TCP)
    sockaddr = addrs[0][4]
    server_class.address_family = addrs[0][0]
    server = server_class(sockaddr[0:2], ThreadedTCPRequestHandler)
    ip, port = server.server_address[0:2]
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.start()
    writePortInFile(port)
    print("Listening on port {0}".format(port))
    try:
        while server_thread.is_alive():
            server_thread.join(1)
    except (KeyboardInterrupt, SystemExit):
        server.shutdown()
if __name__ == "__main__":
    main("localhost", 0)
