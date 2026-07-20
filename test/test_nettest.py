import socket

HOST = "0.0.0.0"   # dengar di semua interface (WAJIB, bukan 127.0.0.1)
PORT = 7777

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((HOST, PORT))
s.listen(1)
print(f"Echo server listening on {HOST}:{PORT}")

while True:
    conn, addr = s.accept()
    print("client:", addr)
    data = conn.recv(1024)
    if data:
        print("recv:", data)
        conn.sendall(data)   # echo balik
    conn.close()