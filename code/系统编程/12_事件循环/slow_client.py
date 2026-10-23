import socket, sys, time

def one(port, tag, line, wait):
    s = socket.create_connection(("127.0.0.1", port))
    t0 = time.monotonic()
    s.sendall(line)
    s.settimeout(wait + 5)
    data = s.recv(4096)
    dt = time.monotonic() - t0
    print(f"{tag}: {dt:.2f}s -> {data.decode().strip()}")
    s.close()

port = int(sys.argv[1])
mode = sys.argv[2]
import threading
if mode == "block":
    a = threading.Thread(target=one, args=(port, "A(slow)", b"slow\n", 3))
    a.start()
    time.sleep(0.3)
    b = threading.Thread(target=one, args=(port, "B(echo)", b"hello\n", 3))
    b.start()
    a.join(); b.join()
else:
    a = threading.Thread(target=one, args=(port, "A(slow)", b"slow\n", 3))
    a.start()
    time.sleep(0.3)
    b = threading.Thread(target=one, args=(port, "B(echo)", b"hello\n", 3))
    b.start()
    a.join(); b.join()
