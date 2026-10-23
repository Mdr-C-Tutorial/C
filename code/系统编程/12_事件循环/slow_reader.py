import socket, sys, time

port = int(sys.argv[1])
first = int(sys.argv[2])   # lines sent before the pause
extra = int(sys.argv[3])   # lines sent while the peer is not reading
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
s.connect(("127.0.0.1", port))
line = b"q" * 60 + b"\n"
s.sendall(line * first)
time.sleep(0.6)
s.sendall(line * extra)
time.sleep(float(sys.argv[4]))
s.settimeout(10)
total = 0
try:
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        total += len(chunk)
except socket.timeout:
    print("timeout")
except ConnectionResetError:
    print("connection reset")
print("received", total, "of", (first + extra) * len(line))
s.close()
