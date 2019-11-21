#!/usr/bin/python

import sys
import string
import socket
from time import sleep


fdr = open("client-input1.dat", 'r')
fdw = open("server-output.dat", 'w+')
#data = string.digits + string.lowercase + string.uppercase

def server(port):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    s.bind(('0.0.0.0', int(port)))
    s.listen(3)
    
    cs, addr = s.accept()
    print addr
    
    while True:
        data = cs.recv(1000)
        print(type(data))
        if data:
            #data = 'server echoes: ' + data
            data = data + '\n'
            print(data)
            fdw.write(data)
            cs.send(data)
            fdw.flush()
        else:
            break
    fdw.close()
    s.close()


def client(ip, port):
    s = socket.socket()
    s.connect((ip, int(port)))
    content = fdr.readlines()
    print(type(content))
    print(content)
    print(content[0][0:-1])
    print(content[1][0:-1])
    for i in range(len(content)):
        s.send(content[i][0:-1])
        print s.recv(1000)
        sleep(1)
    
    fdr.close()
    s.close()

if __name__ == '__main__':
    if sys.argv[1] == 'server':
        server(sys.argv[2])
    elif sys.argv[1] == 'client':
        client(sys.argv[2], sys.argv[3])
