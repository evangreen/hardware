import socket
import sys;

argv = sys.argv
UDP_PORT = 8080

if len(argv) < 3:
    UDP_IP = "192.168.1.101"
    if len(argv) < 2:
        MESSAGE = "FF00FF,FF0000,00FF00,0000FF,00FFFF,FFFF00,FFFFFF,808080,FF8000,FF99CC,FFCCE5,202020,4C9900,FF99FF,FF007F,9999FF"

    else:
        MESSAGE = argv[1];

else:
    UDP_IP = argv[1]
    MESSAGE = argv[2]

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM);
sock.sendto(MESSAGE + "\r\n", (UDP_IP, UDP_PORT))
