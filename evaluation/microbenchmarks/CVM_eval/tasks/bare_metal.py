#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from dataclasses import dataclass
from typing import Any, Optional
from pathlib import Path

from invoke import task
from config import PROJECT_ROOT


@dataclass
class HostResource:
    cpu: int
    memory: int  # GB
    pin_base: int


# Host resources configuration
HOST_RESOURCES = {}
HOST_RESOURCES["vislor"] = {
    "small": HostResource(cpu=1, memory=8, pin_base=8),
    "medium": HostResource(cpu=8, memory=64, pin_base=8),
    "large": HostResource(cpu=32, memory=256, pin_base=0),
}


def get_host_resource(hostname: str, name: str) -> HostResource:
    """Get host resource configuration"""
    return HOST_RESOURCES[hostname][name]


def run_ping_baremetal(name: str, **kargs: Any):
    """Run ping benchmark on bare metal"""
    from network import run_ping_baremetal

    run_ping_baremetal(name)


def run_iperf_baremetal(name: str, udp: bool = False, **kargs: Any):
    """Run iperf benchmark on bare metal"""
    cfg = kargs["config"]  # full config dict
    from network import run_iperf_baremetal

    run_iperf_baremetal(name, udp=udp, config=cfg)


def run_memtier_baremetal(name: str, server: str = "redis", **kargs: Any):
    """Run memtier benchmark on bare metal"""
    cfg = kargs["config"]  # full config dict
    tls: bool = kargs["config"].get("tls", False)
    from network import run_memtier_baremetal

    run_memtier_baremetal(name, server=server, tls=tls, config=cfg)


def run_fio_baremetal(name: str, **kargs: Any):
    """Run fio benchmark on bare metal"""
    cfg = kargs["config"]  # full config dict
    fio_job = kargs["config"]["fio_job"]
    filename = kargs["config"].get("fio_filename", "/tmp/testfile")
    from storage import run_fio_baremetal

    run_fio_baremetal(name, job=fio_job, filename=filename, config=cfg)


def do_action(action: str, **kwargs: Any) -> None:
    """Execute the specified bare metal benchmark action"""
    if action == "run-ping":
        run_ping_baremetal(**kwargs)
    elif action == "run-iperf":
        run_iperf_baremetal(**kwargs)
    elif action == "run-iperf-udp":
        run_iperf_baremetal(udp=True, **kwargs)
    elif action == "run-memtier":
        run_memtier_baremetal(server="redis", **kwargs)
    elif action == "run-memtier-memcached":
        run_memtier_baremetal(server="memcached", **kwargs)
    elif action == "run-fio":
        run_fio_baremetal(**kwargs)
    else:
        raise ValueError(f"Unknown action: {action}")


@task
def start(
    ctx: Any,
    size: str = "medium",  # small, medium, large, full
    hostname: str = None,  # by default use the local hostname
    action: str = "run-ping",
    # Network benchmark options
    target_ip: str = "127.0.0.1",  # Target IP for network benchmarks
    # Storage benchmark options
    fio_job: str = "test",
    fio_filename: str = "/tmp/testfile",
    # Application bench options
    repeat: int = 1,
    tls: bool = False,
    name_extra: str = "",
) -> None:
    """Run bare metal benchmarks

    Examples:
        inv bare-metal.start --action run-ping --name-extra "-baseline"
        inv bare-metal.start --action run-iperf --size large --name-extra "-test"
        inv bare-metal.start --action run-fio --fio-filename "/dev/nvme0n1" --name-extra "-nvme"
        inv bare-metal.start --action run-memtier --tls --name-extra "-tls-test"
    """
    if hostname is None:
        import socket

        hostname = socket.gethostname()

    # Build configuration
    config: dict = {
        "hostname": hostname,
        "size": size,
        "action": action,
        "target_ip": target_ip,
        "fio_job": fio_job,
        "fio_filename": fio_filename,
        "repeat": repeat,
        "tls": tls,
        "name_extra": name_extra,
    }

    # Get host resource configuration
    try:
        resource: HostResource = get_host_resource(hostname, size)
        config["resource"] = resource
    except KeyError:
        print(f"Warning: No resource configuration found for {hostname}/{size}")
        # Use default resource
        config["resource"] = HostResource(cpu=8, memory=64, pin_base=8)

    # Generate benchmark name
    name = f"baremetal-{size}" + name_extra
    print(f"Running bare metal benchmark: {name}")
    print(f"Action: {action}")
    print(f"Resource: {config['resource']}")

    # Execute the benchmark
    do_action(action, name=name, config=config)


# Additional utility task for listing available actions
@task
def list_actions(ctx):
    """List available bare metal benchmark actions"""
    actions = [
        "run-ping",
        "run-iperf",
        "run-iperf-udp",
        "run-memtier",
        "run-memtier-memcached",
        "run-fio",
    ]

    print("Available bare metal benchmark actions:")
    for action in actions:
        print(f"  - {action}")


# Task for checking host resources
@task
def check_resources(ctx, hostname: str = None):
    """Check available host resource configurations"""
    if hostname is None:
        import socket

        hostname = socket.gethostname()

    if hostname in HOST_RESOURCES:
        print(f"Available resource configurations for {hostname}:")
        for size, resource in HOST_RESOURCES[hostname].items():
            print(
                f"  {size}: {resource.cpu} CPUs, {resource.memory}GB RAM, pin_base={resource.pin_base}"
            )
    else:
        print(f"No resource configurations found for hostname: {hostname}")
        print(f"Available hostnames: {list(HOST_RESOURCES.keys())}")
