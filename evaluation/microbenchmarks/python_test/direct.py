import subprocess
import time

N = 1000000  # Number of queries to send

# Direct server: Always returns "true"
direct_server_code = """\
import sys

while True:
    line = sys.stdin.readline().strip()
    if not line:
        break
    print("true", flush=True)
"""

# Direct client: Sends random key-value pairs and measures performance
direct_client_code = f"""\
import sys, time, random, string

N = {N}

def random_string(size):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size))

key = random_string(16)
value = random_string(64)

start_time = time.time()
for _ in range(N):
    query = f"{{key}}{{value}}\\n"

    sys.stdout.write(query)
    sys.stdout.flush()

    response = sys.stdin.readline().strip()
    assert response == "true", "Unexpected response"
end_time = time.time()
# print to stderr as the stdout is redirected to the server
print(f"Direct client time: {{end_time - start_time:.4f}} seconds", file=sys.stderr)
"""

def main():
    print("Starting direct server...")
    server_proc = subprocess.Popen(["python3", "-c", direct_server_code], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    time.sleep(1)  # Let the server initialize

    print("Starting direct client...")
    client_proc = subprocess.run(["python3", "-c", direct_client_code], stdin=server_proc.stdout, stdout=server_proc.stdin, text=True)

    server_proc.kill()
    print("Direct setup complete.")

if __name__ == "__main__":
    main()
