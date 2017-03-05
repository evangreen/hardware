import socket
import sys;

argv = sys.argv
UDP_PORT = 8080

if len(argv) < 3:
    UDP_IP = "192.168.1.147"
    if len(argv) < 2:
        #          0,     10     30     40     50     60     70     80     90     100    120    rain?  rain!  snow?  snow!
        MESSAGE = "FF8000,0000FF,3008FF,6000FF,A00090,C010C0,C080A0,E03040,FF0020,FF0000,FF3000,004000,30FF00,008080,40FFFF"

    else:
        MESSAGE = argv[1];

else:
    UDP_IP = argv[1]
    MESSAGE = argv[2]

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM);
sock.sendto(MESSAGE + "\r\n", (UDP_IP, UDP_PORT))
