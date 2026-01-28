from __future__ import print_function
import socket
import sys
import time
import threading
import re
try:
    import socketserver
except ImportError:
    import SocketServer as socketserver
class ThreadedTCPRequestHandler(socketserver.BaseRequestHandler):
    def process_msgs(self, msgbuf):
        while True:
            (line, sep, rest) = msgbuf.partition('\n')
            if sep == '':
                return line
            msgbuf = rest
            response = ''
            if line.find('Xcmdbuf') > 0:
                name = line.split('"')[1]
                response = '1:putBufferNumber!15 "' + name + '"\n'
                response += '1:startDocumentListen!16\n'
            elif re.match('1:insert=.* "\\\\n"', line):
                cmd = re.search('.*"(.*)"', self.prev_line).group(1)
                testmap = {
                  'getCursor_Test' : '0:getCursor/30\n',
                  'E627_Test' : '0 setReadOnly!31\n',
                  'E628_Test' : '0:setReadOnly 32\n',
                  'E632_Test' : '0:getLength/33\n',
                  'E633_Test' : '0:getText/34\n',
                  'E634_Test' : '0:remove/35 1 1\n',
                  'E635_Test' : '0:insert/36 0 "line1\\n"\n',
                  'E636_Test' : '0:create!37\n',
                  'E637_Test' : '0:startDocumentListen!38\n',
                  'E638_Test' : '0:stopDocumentListen!39\n',
                  'E639_Test' : '0:setTitle!40 "Title"\n',
                  'E640_Test' : '0:initDone!41\n',
                  'E641_Test' : '0:putBufferNumber!42 "XSomeBuf"\n',
                  'E642_Test' : '9:putBufferNumber!43 "XInvalidBuf"\n',
                  'E643_Test' : '0:setFullName!44 "XSomeBuf"\n',
                  'E644_Test' : '0:editFile!45 "Xfile3"\n',
                  'E645_Test' : '0:setVisible!46 T\n',
                  'E646_Test' : '0:setModified!47 T\n',
                  'E647_Test' : '0:setDot!48 1/1\n',
                  'E648_Test' : '0:close!49\n',
                  'E650_Test' : '0:defineAnnoType!50 1 "abc" "a" "a" 1 1\n',
                  'E651_Test' : '0:addAnno!51 1 1 1 1\n',
                  'E652_Test' : '0:getAnno/52 8\n',
                  'editFile_Test' : '2:editFile!53 "Xfile3"\n',
                  'getLength_Test' : '2:getLength/54\n',
                  'getModified_Test' : '2:getModified/55\n',
                  'getText_Test' : '2:getText/56\n',
                  'setDot_Test' : '2:setDot!57 3/6\n',
                  'setDot2_Test' : '2:setDot!57 9\n',
                  'startDocumentListen_Test' : '2:startDocumentListen!58\n',
                  'stopDocumentListen_Test' : '2:stopDocumentListen!59\n',
                  'define_anno_Test' : '2:defineAnnoType!60 1 "s1" "x" "=>" blue none\n',
                  'E532_Test' : '2:defineAnnoType!61 1 "s1" "x" "=>" aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa none\n',
                  'add_anno_Test' : '2:addAnno!62 1 1 2/1 0\n',
                  'get_anno_Test' : '2:getAnno/63 1\n',
                  'remove_anno_Test' : '2:removeAnno!64 1\n',
                  'getModifiedAll_Test' : '0:getModified/65\n',
                  'create_Test' : '3:create!66\n',
                  'setTitle_Test' : '3:setTitle!67 "Xfile4"\n',
                  'setFullName_Test' : '3:setFullName!68 "Xfile4"\n',
                  'initDone_Test' : '3:initDone!69\n',
                  'setVisible_Test' : '3:setVisible!70 T\n',
                  'setModtime_Test' : '3:setModtime!71 6\n',
                  'insert_Test' : '3:insert/72 0 "line1\\nline2\\n"\n',
                  'remove_Test' : '3:remove/73 3 4\n',
                  'remove_invalid_offset_Test' : '3:remove/74 900 4\n',
                  'remove_invalid_count_Test' : '3:remove/75 1 800\n',
                  'guard_Test' : '3:guard!76 8 7\n',
                  'setModified_Test' : '3:setModified!77 T\n',
                  'setModifiedClear_Test' : '3:setModified!77 F\n',
                  'insertDone_Test' : '3:insertDone!78 T F\n',
                  'saveDone_Test' : '3:saveDone!79\n',
                  'invalidcmd_Test' : '3:invalidcmd!80\n',
                  'invalidfunc_Test' : '3:invalidfunc/81\n',
                  'removeAnno_fail_Test' : '0:removeAnno/82 1\n',
                  'guard_fail_Test' : '0:guard/83 1 1\n',
                  'save_fail_Test' : '0:save/84\n',
                  'netbeansBuffer_fail_Test' : '0:netbeansBuffer/85 T\n',
                  'setExitDelay_Test' : '0:setExitDelay!86 2\n',
                  'setReadOnly_Test' : '3:setReadOnly!87 T\n',
                  'setReadOnlyClear_Test' : '3:setReadOnly!88 F\n',
                  'save_Test' : '3:save!89\n',
                  'close_Test' : '3:close!90\n',
                  'specialKeys_Test' : '0:specialKeys!91 "F12 F13 C-F13"\n',
                  'nbbufwrite_Test' : '4:editFile!92 "XnbBuffer"\n4:netbeansBuffer!93 T\n',
                  'startAtomic_Test' : '0:startAtomic!94\n',
                  'endAtomic_Test' : '0:endAtomic!95\n',
                  'AnnoScale_Test' : "".join(['2:defineAnnoType!60 ' + str(i) + ' "s' + str(i) + '" "x" "=>" blue none\n' for i in range(2, 26)]),
                  'detach_Test' : '2:close!96\n1:close!97\nDETACH\n'
                }
                if cmd not in testmap:
                  print("=== invalid command %s ===" % (cmd))
                else:
                  response = testmap[cmd]
            elif line.find('disconnect') > 0:
                self.server.shutdown()
                return
            self.prev_line = line
            if len(response) > 0:
                self.request.sendall(response.encode('utf-8'))
                with open("Xnetbeans", "a") as myfile:
                    myfile.write('send: ' + response)
                if self.debug:
                    with open("save_Xnetbeans", "a") as myfile:
                        myfile.write('send: ' + response)
    def handle(self):
        print("=== socket opened ===")
        self.debug = 0
        self.prev_line = ''
        msgbuf = ''
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
            with open("Xnetbeans", "a") as myfile:
                myfile.write(received)
            if self.debug:
                with open("save_Xnetbeans", "a") as myfile:
                    myfile.write(received)
            msgbuf += received
            msgbuf = self.process_msgs(msgbuf)
class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    pass
def writePortInFile(port):
    f = open("Xportnr", "w")
    f.write("{0}".format(port))
    f.close()
if __name__ == "__main__":
    HOST, PORT = "localhost", 0
    addrs = socket.getaddrinfo(HOST, PORT, 0, 0, socket.IPPROTO_TCP)
    sockaddr = addrs[0][4]
    ThreadedTCPServer.address_family = addrs[0][0]
    server = ThreadedTCPServer(sockaddr[0:2], ThreadedTCPRequestHandler)
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
