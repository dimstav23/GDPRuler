import subprocess
from datetime import datetime
from pathlib import Path
import subprocess
import time
from typing import Optional

from config import PROJECT_ROOT, VM_IP
from qemu import QemuVm


def run_ping(name: str, vm: QemuVm, pin_base=20):
    """Ping the VM.
    The results are saved in ./bench-result/network/ping/{name}/{date}
    """
    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/network/ping/{name}/{date}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    for pkt_size in [64, 128, 256, 512, 1024]:
        process = subprocess.Popen(
            f"taskset -c {pin_base} ping -c 300 -i0.1 -s {pkt_size} {VM_IP}".split(" "),
            stderr=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )

        stdout, stderr = process.communicate()
        exit_code = process.wait()

        if exit_code != 0:
            print(f"Error running ping: {stderr}")
            continue

        with open(outputdir_host / f"{pkt_size}.log", "wb") as f:
            f.write(stdout)

    print(f"Results saved in {outputdir_host}")


def run_iperf(
    name: str,
    vm: QemuVm,
    udp: bool = False,
    port: int = 7175,
    parallel: Optional[int] = None,
    pin_start: int = 20,
    pin_end: Optional[int] = None,
):
    """Run the iperf benchmark on the VM.
    The results are saved in ./bench-result/network/iperf/{name}/{proto}/{date}/
    """
    if udp:
        proto = "udp"
        pkt_sizes = [64, 128, 256, 512, 1024, 1460]
        if parallel is None:
            parallel = 8
    else:
        pkt_sizes = ["256", "4K", "32K", "128K"]
        proto = "tcp"
        if parallel is None:
            parallel = 32
    if pin_end is None:
        pin_end = pin_start + parallel - 1

    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/network/iperf/{name}/{proto}/{date}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    # start server
    server_cmd = ["iperf3", "-s", "-p", f"{port}", "-D"]
    vm.ssh_cmd(server_cmd)
    time.sleep(1)

    # run client
    for pkt_size in pkt_sizes:
        cmd = [
            "taskset",
            "-c",
            f"{pin_start}-{pin_end}",
            "iperf3",
            "-c",
            f"{VM_IP}",
            "-p",
            f"{port}",
            "-b",
            "0",
            "-i",
            "1",
            "-l",
            f"{pkt_size}",
            "-P",
            f"{parallel}",
        ]
        if udp:
            cmd.append("-u")
        print(cmd)
        output = subprocess.check_output(cmd).decode()
        lines = output.split("\n")
        with open(outputdir_host / f"{pkt_size}.log", "w") as f:
            f.write("\n".join(lines))

        # workaround to avoid "iperf3: error - unable to receive control message - port may not be available"
        time.sleep(1)

    print(f"Results saved in {outputdir_host}")


def run_memtier(
    name: str,
    vm: QemuVm,
    server: str = "redis",
    port: int = 6379,
    tls_port: int = 6380,
    tls: bool = False,
    server_threads: Optional[int] = None,
    client_threads: Optional[int] = None,
    client_key: str = PROJECT_ROOT / "benchmarks/network/tls/pki/private/client.key",
    client_cert: str = PROJECT_ROOT / "benchmarks/network/tls/pki/issued/client.crt",
    ca_cert: str = PROJECT_ROOT / "benchmarks/network/tls/pki/ca.crt",
    pin_start: int = 20,
    pin_end: Optional[int] = None,
):
    """Run the memtier benchmark on the VM using redis or memcached.
    `server_threads` is only valid for memcached.
    The results are saved in ./bench-result/network/memtier/{server}[-tls]/{name}/{date}/
    """
    if tls:
        tls_ = "-tls"
    else:
        tls_ = ""
    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/network/memtier/{server}{tls_}/{name}/{date}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    if server == "redis":
        proto = "redis"
        prepare_redis_cmd = [
            "rm",
            "-rf",
            "/root/dump.rdb",
            "/share/benchmarks/network/dump.rdb",
        ]
        vm.ssh_cmd(prepare_redis_cmd)
    elif server == "memcached":
        proto = "memcache_binary"
    else:
        raise ValueError(f"Unknown server: {server}")

    if server_threads is None:
        if "resource" in vm.config:
            server_threads = vm.config["resource"].cpu
        else:
            server_threads = 1

    if client_threads is None:
        if server == "redis":
            client_threads = 8
        else:
            client_threads = vm.config["resource"].cpu

    if pin_end is None:
        pin_end = pin_start + client_threads - 1

    server_cmd = [
        "just",
        "-f",
        "/share/benchmarks/network/justfile",
        f"STANDARD_MEMTIER_PORT={port}",
        f"TLS_MEMTIER_PORT={tls_port}",
        f"THREADS={server_threads}",
        f"run-{server}{tls_}",
    ]
    vm.ssh_cmd(server_cmd)
    print("Server started")
    time.sleep(1)

    if tls:
        cmd = [
            "taskset",
            "-c",
            f"{pin_start}-{pin_end}",
            "memtier_benchmark",
            f"--host={VM_IP}",
            "-p",
            f"{tls_port}",
            "-t",
            f"{client_threads}",
            "-c",
            "100",
            "--pipeline=40",
            f"--protocol={proto}",
            "--tls",
            f"--cert={client_cert}",
            f"--key={client_key}",
            f"--cacert={ca_cert}",
        ]
    else:
        cmd = [
            "taskset",
            "-c",
            f"{pin_start}-{pin_end}",
            "memtier_benchmark",
            f"--host={VM_IP}",
            "-p",
            f"{port}",
            "-t",
            f"{client_threads}",
            "-c",
            "100",
            "--pipeline=40",
            f"--protocol={proto}",
        ]
    print(cmd)
    output = subprocess.check_output(cmd).decode()
    lines = output.split("\n")
    with open(outputdir_host / f"memtier.log", "w") as f:
        f.write("\n".join(lines))

    print(f"Results saved in {outputdir_host}")


def run_nginx(
    name: str,
    vm: QemuVm,
    threads: int = 8,
    connections: int = 300,
    duration: str = "30s",
    pin_start: int = 20,
    pin_end: Optional[int] = None,
):
    """Run the nginx on the VM and the wrk benchmark on the host.
    The results are saved in ./bench-result/network/nginx/{name}/{date}/
    """
    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/network/nginx/{name}/{date}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    if pin_end is None:
        pin_end = pin_start + threads - 1

    server_cmd = ["just", "-f", "/share/benchmarks/network/justfile", "run-nginx"]
    vm.ssh_cmd(server_cmd)
    time.sleep(1)

    # HTTP
    cmd = [
        "taskset",
        "-c",
        f"{pin_start}-{pin_end}",
        "wrk",
        f"http://{VM_IP}",
        f"-t{threads}",
        f"-c{connections}",
        f"-d{duration}",
    ]
    print(cmd)
    output = subprocess.run(cmd, capture_output=True, text=True)
    if output.returncode != 0:
        print(f"Error running wrk: {output.stderr}")
    lines = output.stdout.split("\n")
    with open(outputdir_host / f"http.log", "w") as f:
        f.write("\n".join(lines))

    # HTTPS
    cmd = [
        "taskset",
        "-c",
        f"{pin_start}-{pin_end}",
        "wrk",
        f"https://{VM_IP}",
        f"-t{threads}",
        f"-c{connections}",
        f"-d{duration}",
    ]
    print(cmd)
    output = subprocess.run(cmd, capture_output=True, text=True)
    if output.returncode != 0:
        print(f"Error running wrk: {output.stderr}")
    lines = output.stdout.split("\n")
    with open(outputdir_host / f"https.log", "w") as f:
        f.write("\n".join(lines))

    print(f"Results saved in {outputdir_host}")


################################################################
##################### Bare metal execution #####################
################################################################


def run_ping_baremetal(name: str, target_ip: str = "127.0.0.1", pin_base=20):
    """Ping benchmark on bare metal"""
    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/network/ping/{name}/{date}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    for pkt_size in [64, 128, 256, 512, 1024]:
        process = subprocess.Popen(
            f"taskset -c {pin_base} ping -c 300 -i0.1 -s {pkt_size} {target_ip}".split(
                " "
            ),
            stderr=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )

        stdout, stderr = process.communicate()
        exit_code = process.wait()

        if exit_code != 0:
            print(f"Error running ping: {stderr}")
            continue

        with open(outputdir_host / f"{pkt_size}.log", "wb") as f:
            f.write(stdout)

    print(f"Results saved in {outputdir_host}")


def run_iperf_baremetal(
    name: str,
    udp: bool = False,
    port: int = 7175,
    parallel: Optional[int] = None,
    pin_start: int = 20,
    pin_end: Optional[int] = None,
    target_ip: str = "127.0.0.1",
    **kwargs,
):
    """Run iperf benchmark on bare metal (client and server on same host)"""
    if udp:
        proto = "udp"
        pkt_sizes = [64, 128, 256, 512, 1024, 1460]
        if parallel is None:
            parallel = 8
    else:
        pkt_sizes = ["256", "4K", "32K", "128K"]
        proto = "tcp"
        if parallel is None:
            parallel = 32

    if pin_end is None:
        pin_end = pin_start + parallel - 1

    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/network/iperf/{name}/{proto}/{date}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    # Get server CPU configuration from resource
    resource = kwargs.get("config", {}).get("resource")
    if resource:
        server_total_cpus = resource.cpu
        server_pin_base = resource.pin_base
        # For FIO, use all available CPUs
        server_cpu_range = (
            f"{server_pin_base}-{server_pin_base + server_total_cpus - 1}"
        )
    else:
        server_cpu_range = "8-15"  # default range

    # Start server in background
    server_cmd = [
        "taskset",
        "-c",
        server_cpu_range,
        "iperf3",
        "-s",
        "-p",
        f"{port}",
        "-D",
    ]
    subprocess.run(server_cmd)
    time.sleep(1)

    # Run client
    for pkt_size in pkt_sizes:
        cmd = [
            "taskset",
            "-c",
            f"{pin_start}-{pin_end}",
            "iperf3",
            "-c",
            target_ip,
            "-p",
            f"{port}",
            "-b",
            "0",
            "-i",
            "1",
            "-l",
            f"{pkt_size}",
            "-P",
            f"{parallel}",
        ]
        if udp:
            cmd.append("-u")

        print(cmd)
        output = subprocess.check_output(cmd).decode()
        lines = output.split("\n")
        with open(outputdir_host / f"{pkt_size}.log", "w") as f:
            f.write("\n".join(lines))

        time.sleep(1)

    # Kill server
    subprocess.run(["pkill", "iperf3"])
    print(f"Results saved in {outputdir_host}")


def run_memtier_baremetal(
    name: str,
    server: str = "redis",
    port: int = 6379,
    tls_port: int = 6380,
    tls: bool = False,
    server_threads: Optional[int] = None,
    client_threads: Optional[int] = None,
    client_key: str = PROJECT_ROOT / "benchmarks/network/tls/pki/private/client.key",
    client_cert: str = PROJECT_ROOT / "benchmarks/network/tls/pki/issued/client.crt",
    ca_cert: str = PROJECT_ROOT / "benchmarks/network/tls/pki/ca.crt",
    pin_start: int = 20,
    pin_end: Optional[int] = None,
    target_ip: str = "127.0.0.1",
    **kwargs,
):
    """Run the memtier benchmark on bare metal using redis or memcached.
    `server_threads` is only valid for memcached.
    The results are saved in ./bench-result/network/memtier/{server}[-tls]/{name}/{date}/
    """
    if tls:
        tls_ = "-tls"
    else:
        tls_ = ""

    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/network/memtier/{server}{tls_}/{name}/{date}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    if server == "redis":
        proto = "redis"
        prepare_redis_cmd = [
            "rm",
            "-rf",
            str(PROJECT_ROOT / "benchmarks/network/dump.rdb"),
        ]
        subprocess.run(prepare_redis_cmd, check=False)
    elif server == "memcached":
        proto = "memcache_binary"
    else:
        raise ValueError(f"Unknown server: {server}")

    # Get server_threads from config if available
    if server_threads is None:
        if "config" in kwargs and "resource" in kwargs["config"]:
            server_threads = kwargs["config"]["resource"].cpu
        else:
            server_threads = 1

    if client_threads is None:
        if server == "redis":
            client_threads = 8
        else:
            if "config" in kwargs and "resource" in kwargs["config"]:
                client_threads = kwargs["config"]["resource"].cpu
            else:
                client_threads = 8

    if pin_end is None:
        pin_end = pin_start + client_threads - 1

    # Get server CPU configuration from resource
    resource = kwargs.get("config", {}).get("resource")
    if resource:
        server_total_cpus = resource.cpu
        server_pin_base = resource.pin_base
        # For FIO, use all available CPUs
        server_cpu_range = (
            f"{server_pin_base}-{server_pin_base + server_total_cpus - 1}"
        )
    else:
        server_cpu_range = "8-15"  # default range

    # Start server using justfile (adapted for bare metal)
    server_cmd = [
        "taskset",
        "-c",
        server_cpu_range,
        "just",
        "-f",
        f"{PROJECT_ROOT}/benchmarks/network/justfile",
        f"STANDARD_MEMTIER_PORT={port}",
        f"TLS_MEMTIER_PORT={tls_port}",
        f"THREADS={server_threads}",
        f"run-{server}{tls_}",
    ]

    print(f"Starting server: {' '.join(server_cmd)}")
    server_process = subprocess.Popen(server_cmd)
    print("Server started")
    time.sleep(2)  # Wait for server to start

    try:
        # Run memtier client
        if tls:
            cmd = [
                "taskset",
                "-c",
                f"{pin_start}-{pin_end}",
                "memtier_benchmark",
                f"--host={target_ip}",
                "-p",
                f"{tls_port}",
                "-t",
                f"{client_threads}",
                "-c",
                "100",
                "--pipeline=40",
                f"--protocol={proto}",
                "--tls",
                f"--cert={client_cert}",
                f"--key={client_key}",
                f"--cacert={ca_cert}",
            ]
        else:
            cmd = [
                "taskset",
                "-c",
                f"{pin_start}-{pin_end}",
                "memtier_benchmark",
                f"--host={target_ip}",
                "-p",
                f"{port}",
                "-t",
                f"{client_threads}",
                "-c",
                "100",
                "--pipeline=40",
                f"--protocol={proto}",
            ]

        print(cmd)
        output = subprocess.check_output(cmd).decode()
        lines = output.split("\n")
        with open(outputdir_host / f"memtier.log", "w") as f:
            f.write("\n".join(lines))

    finally:
        # Clean up: kill server process and any remaining processes
        try:
            server_process.terminate()
            server_process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server_process.kill()

        # Also kill any remaining server processes (backup cleanup)
        subprocess.run(["pkill", "-f", server], check=False)

    print(f"Results saved in {outputdir_host}")
