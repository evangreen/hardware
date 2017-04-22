import socket
import sys;

argv = sys.argv
UDP_PORT = 8080

if len(argv) < 3:
    UDP_IP = "192.168.1.143"
    if len(argv) < 2:
        # Format: PortA,PortB,Speed,RPM,Fuel,Oil,Temp.
        # Default (indicators off, ignition on, small gauges north):
        # "70,1080,0,0,68,C0,78"
        # The message below is the default plus L and R turn signals, and
        #RPM at 2000
        MESSAGE = "70,7080,0,254,68,C0,78"

    else:
        MESSAGE = argv[1];

else:
    UDP_IP = argv[1]
    MESSAGE = argv[2]

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM);
sock.sendto(MESSAGE + "\r\n", (UDP_IP, UDP_PORT))
