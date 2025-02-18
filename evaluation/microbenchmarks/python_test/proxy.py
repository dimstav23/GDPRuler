import subprocess
import time

N = 1000000  # Number of queries to send

# Proxy server: Forwards requests and always returns "true"
proxy_server_code = """\
import socket, threading

HOST, PORT = "127.0.0.1", 5000

def handle_client(client_socket):
    while True:
        data = client_socket.recv(1024)
        if not data:
            break
        client_socket.sendall(b"true\\n")
    client_socket.close()

def main():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((HOST, PORT))
    server_socket.listen(5)
    
    print("Proxy server listening on", HOST, PORT)
    
    while True:
        client_socket, _ = server_socket.accept()
        threading.Thread(target=handle_client, args=(client_socket,)).start()

if __name__ == "__main__":
    main()
"""

# Proxy client: Sends queries via the proxy
proxy_client_code = f"""\
import socket, time, random, string

N = {N}
PROXY_HOST, PROXY_PORT = "127.0.0.1", 5000

def random_string(size):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size))

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((PROXY_HOST, PROXY_PORT))

key = random_string(16)
value = random_string(64)
start_time = time.time()

for _ in range(N):    
    query = f"{{key}}{{value}}\\n".encode()

    sock.sendall(query)
    response = sock.recv(1024).decode().strip()
    assert response == "true", "Unexpected response"

end_time = time.time()
print(f"Proxy client time: {{end_time - start_time:.4f}} seconds")

sock.close()
"""

def main():
    print("Starting proxy server...")
    proxy_proc = subprocess.Popen(["python3", "-c", proxy_server_code])

    time.sleep(1)  # Let the server initialize

    print("Starting proxy client...")
    client_proc = subprocess.run(["python3", "-c", proxy_client_code])

    proxy_proc.kill()
    print("Proxy setup complete.")

if __name__ == "__main__":
    main()

