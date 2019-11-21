#!/usr/bin/python

import sys
import string
import socket
from time import sleep

data = string.digits + string.lowercase + string.uppercase

def server(port):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    s.bind(('0.0.0.0', int(port)))
    s.listen(3)
    
    cs, addr = s.accept()
    print addr
    
    while True:
        data = cs.recv(1000)
        print('recv YES!')
        print(type(data))
        if data:
            print('data YES!')
            data = 'server echoes: ' + data
            cs.send(data)
        else:
            print('break YES!')
            break
    
    s.close()


def client(ip, port):
    s = socket.socket()
    s.connect((ip, int(port)))
    
    for i in range(10):
        num = s.send(data)
        print(num)
        print s.recv(1000)
        print(num)
        sleep(1)
    
    s.close()

if __name__ == '__main__':
    if sys.argv[1] == 'server':
        server(sys.argv[2])
    elif sys.argv[1] == 'client':
        client(sys.argv[2], sys.argv[3])
