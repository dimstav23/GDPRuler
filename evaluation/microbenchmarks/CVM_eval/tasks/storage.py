#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from datetime import datetime
from pathlib import Path

import time
import subprocess
from config import PROJECT_ROOT
from qemu import QemuVm


def run_fio(
    name: str,
    vm: QemuVm,
    job: str = "test",
    filename: str = "/dev/vda",
):
    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/fio/{name}/{job}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)
    output = Path("/share") / outputdir / f"{date}.json"
    fio_job = f"/share/config/fio/{job}.fio"
    cmd = [
        "fio",
        f"--filename={filename}",
        f"--output={output}",
        "--output-format=json",
        fio_job,
    ]
    vm.ssh_cmd(cmd)


def run_fio_baremetal(
    name: str, job: str = "test", filename: str = "/dev/nvme1n1", **kwargs
):
    """Run fio benchmark on bare metal"""
    date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    outputdir = Path(f"./bench-result/fio/{name}/{job}/")
    outputdir_host = PROJECT_ROOT / outputdir
    outputdir_host.mkdir(parents=True, exist_ok=True)

    # Get CPU configuration from resource
    resource = kwargs.get("config", {}).get("resource")
    if resource:
        total_cpus = resource.cpu
        pin_base = resource.pin_base
        # For FIO, use all available CPUs
        cpu_range = f"{pin_base}-{pin_base + total_cpus - 1}"
    else:
        cpu_range = "8-15"  # default range

    output = outputdir_host / f"{date}.json"
    fio_job = PROJECT_ROOT / f"config/fio/{job}.fio"

    cmd = [
        "sudo",
        "taskset",
        "-c",
        cpu_range,
        "fio",
        f"--filename={filename}",
        f"--output={output}",
        "--output-format=json",
        str(fio_job),
    ]

    print(f"Running FIO with CPU pinning: {cpu_range}")
    print(cmd)
    subprocess.run(cmd)
    print(f"Results saved in {outputdir_host}")


def mount_disk(vm: QemuVm, dev: str, mountpoint: str = "/mnt", format="no") -> bool:
    """Mount a disk on the VM"""
    vm.ssh_cmd(["sudo", "mkdir", "-p", mountpoint])

    if format == "auto":
        # try mount
        output = vm.ssh_cmd(["sudo", "mount", dev, mountpoint], check=False)
        if output.returncode == 0:
            print(f"[mount disk] mount {dev} to {mountpoint}")
            return True
        # if mount fail, then format the disk
        print("[mount disk] format disk")
        vm.ssh_cmd(["sudo", "mkfs.ext4", dev])
    elif format == "yes":
        vm.ssh_cmd(["sudo", "mkfs.ext4", dev])

    time.sleep(1)

    vm.ssh_cmd(["sudo", "mount", dev, mountpoint], check=False)
    if output.returncode == 0:
        print(f"[mount disk] mount {dev} to {mountpoint}")
        return True
    else:
        print(f"[mount disk] mount {dev} to {mountpoint} failed: {output.returncode}")
        return False
