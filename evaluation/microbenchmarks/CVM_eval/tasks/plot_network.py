#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import matplotlib as mpl  # type: ignore
import matplotlib.pyplot as plt  # type: ignore
import seaborn as sns  # type: ignore
from typing import Any, Dict, List, Union, Optional
import pandas as pd
import os
import numpy as np
from pathlib import Path

from invoke import task

mpl.use("Agg")
mpl.rcParams["text.latex.preamble"] = r"\usepackage{amsmath}"
mpl.rcParams["pdf.fonttype"] = 42
mpl.rcParams["ps.fonttype"] = 42
mpl.rcParams["font.family"] = "libertine"

sns.set_style("whitegrid")
sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8})
sns.set_context("paper", rc={"font.size": 5, "axes.titlesize": 5, "axes.labelsize": 8})

# 3.3 inch for single column, 7 inch for double column
figwidth_half = 3.3
figwidth_full = 7

FONTSIZE = 6
TITLE_FONTSIZE = FONTSIZE
LABEL_FONTSIZE = FONTSIZE
TICK_FONTSIZE = FONTSIZE - 1
LEGEND_FONTSIZE = FONTSIZE
ANNOTATION_FONTSIZE = FONTSIZE / 2 + 1

palette = sns.color_palette("pastel", n_colors=15)
hatches = ["", "o", "//", "x", ""]

BENCH_RESULT_DIR = Path("./bench-result/network")

# Define consistent color and hatch mappings for all labels
LABEL_COLORS = {
    "Native": palette[0],
    "VM": palette[1],
    "SNP": palette[2],
    "SNP-poll": palette[3],
    "VM-vhost": palette[4],
    "SNP-vhost": palette[5],
}

LABEL_HATCHES = {
    "Native": "",
    "VM": "o",
    "SNP": "//",
    "SNP-poll": "x",
    "VM-vhost": "",
    "SNP-vhost": "o",
    # Add any other labels you use
}


def parse_iperf_result_sub(
    name: str, mode: str, date: str, lebel: str, pkt_size: [int]
) -> pd.DataFrame:
    ths = []
    for size in pkt_size:
        file = BENCH_RESULT_DIR / "iperf" / name / mode / date / f"{size}.log"
        with file.open("r") as f:
            lines = f.readlines()

        # example format:
        # > [SUM]   0.00-10.00  sec  11.7 GBytes  10.1 Gbits/sec                  receiver
        # find the line with [SUM] from the end
        for line in reversed(lines):
            if "[SUM]" in line:
                break
        else:
            raise ValueError("No [SUM] line found")
        th = float(line.split()[5])
        if line.split()[6] == "Mbits/sec":
            th /= 1000.0
        ths.append(th)

    df = pd.DataFrame({"name": lebel, "size": pkt_size, "throughput": ths})
    return df


# bench mark path:
# ./bench-result/network/iperf/{name}/{date}
def parse_iperf_result(
    name: str, label: str, mode: str, date=None, pkt=None, max_num: int = 10
) -> pd.DataFrame:
    # create df like the following
    # | VM | pkt size | throughput |
    # |----|----------|------------|
    # |    |          |            |
    if pkt is not None:
        pktsize = [pkt]
    elif mode == "udp":
        pktsize = [64, 128, 256, 512, 1024, 1460]
    elif mode == "tcp":
        # pktsize = [64, 128, 256, 512, 1024, "32K", "128K"]
        pktsize = [256, "4K", "32K", "128K"]
    else:
        raise ValueError(f"Invalid mode: {mode}")

    dates = []
    if date is None:
        # use the latest date
        dates = sorted(os.listdir(BENCH_RESULT_DIR / "iperf" / name / mode))[-max_num:]
    else:
        dates.append(date)

    dfs = []
    for date in dates:
        df = parse_iperf_result_sub(name, mode, date, label, pktsize)
        dfs.append(df)

    df = pd.concat(dfs)

    return df


def parse_memtier_result_sub(
    path: str, label: str, server: str, tls: bool = False
) -> pd.DataFrame:
    """
        Example output:
        ```
    Type         Ops/sec     Hits/sec   Misses/sec    Avg. Latency     p50 Latency     p99 Latency   p99.9 Latency       KB/sec
    ----------------------------------------------------------------------------------------------------------------------------
    Sets        47686.59          ---          ---        62.87207        37.37500       716.79900       937.98300      3672.70
    Gets       476341.91         0.00    476341.91        62.40503        37.37500       716.79900       937.98300     18090.40
    Waits           0.00          ---          ---             ---             ---             ---             ---          ---
    Totals     524028.51         0.00    476341.91        62.44752        37.37500       716.79900       937.98300     21763.10
    ```
    """
    print(path)
    if tls:
        tls_ = "(tls)"
    else:
        tls_ = ""
    workloads = []
    ths = []
    with open(path, "r") as f:
        lines = f.readlines()

        for line in lines:
            if line.startswith("Gets"):
                th = float(line.split()[1].strip()) / 1e6
                ths.append(th)
                workloads.append(f"GET{tls_}")
            elif line.startswith("Sets"):
                th = float(line.split()[1].strip()) / 1e6
                ths.append(th)
                workloads.append(f"SET{tls_}")

    df = pd.DataFrame(
        {"name": label, "workload": workloads, "throughput": ths, "server": server}
    )
    return df


def parse_memtier_result(
    name: str, label: str, server: str, date=None, date_tls=None, max_num: int = 10
) -> pd.DataFrame:
    dates = []

    if date is None:
        dates = sorted(os.listdir(BENCH_RESULT_DIR / "memtier" / server / name))[
            -max_num:
        ]
    else:
        dates.append(date)

    dfs = []
    for date in dates:
        df = parse_memtier_result_sub(
            BENCH_RESULT_DIR / "memtier" / server / name / date / "memtier.log",
            label,
            server,
            False,
        )
        dfs.append(df)

        if date_tls is None:
            dir_path = BENCH_RESULT_DIR / "memtier" / f"{server}-tls" / name
            date_tls = (
                sorted(os.listdir(dir_path))[-1]
                if dir_path.exists() and os.listdir(dir_path)
                else None
            )
        if date_tls is not None:
            df_tls = parse_memtier_result_sub(
                BENCH_RESULT_DIR
                / "memtier"
                / f"{server}-tls"
                / name
                / date_tls
                / "memtier.log",
                f"{label}",
                server,
                True,
            )
            dfs.append(df_tls)

    df = pd.concat(dfs)

    return df


@task
def plot_iperf(
    ctx,
    cvm="snp",
    mode="tcp",
    vhost=False,
    mq=False,
    tmebypass=False,
    poll=False,
    plot_all=False,
    outdir="plot",
    outname=None,
    size="medium",
    pkt=None,
    result_dir=None,
):
    if result_dir is not None:
        global BENCH_RESULT_DIR
        BENCH_RESULT_DIR = Path(result_dir)

    bare_metal = "baremetal"
    bare_metal_label = "Native"

    pvm = ""
    pcvm = ""
    if cvm == "snp":
        vm = "amd"
        vm_label = "VM"
        cvm_label = "SNP"
    else:
        vm = "intel"
        vm_label = "vm"
        cvm_label = "td"
        if tmebypass:
            pvm = "-tmebypass"
    if poll:
        pvm += "-poll"
        pcvm += "-poll"

    def get_name(name, p, vhost=False, mq=mq, swiotlb=False):
        n = f"{name}-disk-{size}{p}"
        if vhost:
            n += "-vhost"
        if mq:
            n += "-mq"
        if swiotlb:
            n += "-swiotlb"
        return n

    dfs = []
    dfs.append(
        parse_iperf_result(f"{bare_metal}-{size}", bare_metal_label, mode, pkt=pkt)
    )
    dfs.append(parse_iperf_result(get_name(vm, pvm), vm_label, mode, pkt=pkt))
    dfs.append(
        parse_iperf_result(
            get_name(vm, pvm, vhost=False, swiotlb=True),
            f"{vm_label}-swiotlb",
            mode,
            pkt=pkt,
        )
    )
    dfs.append(
        parse_iperf_result(
            get_name(vm, pvm, vhost=True), f"{vm_label}-vhost", mode, pkt=pkt
        )
    )
    dfs.append(
        parse_iperf_result(
            get_name(vm, pvm, vhost=True, swiotlb=True),
            f"{vm_label}-vhost-swiotlb",
            mode,
            pkt=pkt,
        )
    )
    dfs.append(parse_iperf_result(get_name(cvm, pcvm), cvm_label, mode, pkt=pkt))
    dfs.append(
        parse_iperf_result(
            get_name(cvm, "-haltpoll"), f"{cvm_label}-hpoll", mode, pkt=pkt
        )
    )
    dfs.append(
        parse_iperf_result(get_name(cvm, "-poll"), f"{cvm_label}-poll", mode, pkt=pkt)
    )
    dfs.append(
        parse_iperf_result(
            get_name(cvm, pcvm, vhost=True), f"{cvm_label}-vhost", mode, pkt=pkt
        )
    )
    dfs.append(
        parse_iperf_result(
            get_name(cvm, "-haltpoll-vhost"),
            f"{cvm_label}-vhost-hpoll",
            mode,
            pkt=pkt,
        )
    )
    dfs.append(
        parse_iperf_result(
            get_name(cvm, "-poll-vhost"), f"{cvm_label}-vhost-poll", mode, pkt=pkt
        )
    )
    df = pd.concat(dfs)
    print(df)

    # plot thropughput
    fig, ax = plt.subplots(figsize=(figwidth_half, 2.0))
    sns.barplot(
        x="size",
        y="throughput",
        hue="name",
        data=df,
        ax=ax,
        palette=palette,
        edgecolor="black",
        err_kws={"linewidth": 0.6},
    )
    if pkt is not None:
        ax.set_xticklabels([], fontsize=TICK_FONTSIZE)
        # ax.set_xlabel(f"Buffer Size {pkt} byte")
        ax.set_xlabel("", fontsize=LABEL_FONTSIZE)
    else:
        ax.set_xlabel("Packet Size (byte)", fontsize=LABEL_FONTSIZE)
    ax.set_ylabel("Throughput (Gbps)", fontsize=LABEL_FONTSIZE)
    ax.set_title("Higher is better ↑", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    ax.tick_params(
        axis="x", labelsize=TICK_FONTSIZE, length=3, pad=1
    )  # Remove x-axis tick bars
    ax.tick_params(axis="y", labelsize=TICK_FONTSIZE, pad=2)
    # remove legend title
    ax.get_legend().set_title("")
    ax.grid(True, alpha=0.3, axis="y")

    plt.legend(fontsize=LEGEND_FONTSIZE)

    # annotate values with .2f
    for container in ax.containers:
        ax.bar_label(
            container, fmt="%.2f", rotation=90, padding=2, fontsize=ANNOTATION_FONTSIZE
        )

    # sns.despine(top=True)
    plt.tight_layout()

    if outname is None:
        outname = f"iperf_{mode}"
        if vhost:
            outname += "_vhost"
        if mq:
            outname += "_mq"
        if pkt is not None:
            outname += f"_{pkt}"
        outname += f"_throughput{pvm}"
    if plot_all:
        outname += "_all"
    outname += ".pdf"

    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    # Save as PDF
    save_path_pdf = outdir / outname
    plt.savefig(save_path_pdf, format="pdf", bbox_inches="tight", dpi=300)
    print(f"PDF plot saved in {save_path_pdf}")
    # Save as PNG
    save_path_png = outdir / outname.replace(".pdf", ".png")
    plt.savefig(save_path_png, format="png", bbox_inches="tight", dpi=300)
    print(f"PNG plot saved in {save_path_png}")
    plt.clf()


@task
def plot_redis(
    ctx,
    cvm="snp",
    vhost=False,
    mq=False,
    outdir="plot",
    outname=None,
    size="medium",
    result_dir=None,
):
    if result_dir is not None:
        global BENCH_RESULT_DIR
        BENCH_RESULT_DIR = Path(result_dir)

    bare_metal = "baremetal"
    bare_metal_label = "Native"

    if cvm == "snp":
        vm = "amd"
        vm_label = "VM"
        cvm_label = "SNP"
    else:
        vm = "intel"
        vm_label = "vm"
        cvm_label = "td"

    def get_name(name, vhost=False, p="", mq=mq):
        n = f"{name}-disk-{size}{p}"
        if vhost:
            n += "-vhost"
        if mq:
            n += "-mq"
        return n

    dfs = []
    dfs.append(parse_memtier_result(f"{bare_metal}-{size}", bare_metal_label, "redis"))
    dfs.append(parse_memtier_result(get_name(vm), vm_label, "redis"))
    dfs.append(
        parse_memtier_result(get_name(vm, vhost=True), f"{vm_label}-vhost", "redis")
    )
    dfs.append(parse_memtier_result(get_name(cvm), cvm_label, "redis"))
    dfs.append(
        parse_memtier_result(
            get_name(cvm, p="-haltpoll"), f"{cvm_label}-hpoll", "redis"
        )
    )
    dfs.append(
        parse_memtier_result(get_name(cvm, p="-poll"), f"{cvm_label}-poll", "redis")
    )
    dfs.append(
        parse_memtier_result(get_name(cvm, vhost=True), f"{cvm_label}-vhost", "redis")
    )
    dfs.append(
        parse_memtier_result(
            get_name(cvm, vhost=True, p="-haltpoll"),
            f"{cvm_label}-hpoll-vhost",
            "redis",
        )
    )
    dfs.append(
        parse_memtier_result(
            get_name(cvm, vhost=True, p="-poll"), f"{cvm_label}-poll-vhost", "redis"
        )
    )
    df = pd.concat(dfs)
    print(df)

    fig, ax = plt.subplots(figsize=(figwidth_half, 2.0))
    sns.barplot(
        x="workload",
        y="throughput",
        hue="name",
        data=df,
        ax=ax,
        palette=palette,
        edgecolor="black",
        err_kws={"linewidth": 0.6},
    )
    ax.set_xlabel("", fontsize=LABEL_FONTSIZE)
    ax.set_ylabel("Throughput (M req/s)", fontsize=LABEL_FONTSIZE)
    ax.set_title("Higher is better ↑", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    ax.tick_params(
        axis="x", labelsize=TICK_FONTSIZE, length=3, pad=1
    )  # Remove x-axis tick bars
    ax.tick_params(axis="y", labelsize=TICK_FONTSIZE, pad=2)

    # remove legend title
    ax.get_legend().set_title("")

    # set legend ncol
    ax.legend(loc="center", ncol=2, fontsize=LEGEND_FONTSIZE)
    ax.grid(True, alpha=0.3, axis="y")

    # annotate values with .2f
    for container in ax.containers:
        ax.bar_label(
            container, fmt="%.2f", fontsize=ANNOTATION_FONTSIZE, rotation=90, padding=2
        )

    # sns.despine(top=True)
    plt.tight_layout()

    if outname is None:
        outname = f"redis"
        if vhost:
            outname += "_vhost"
        if mq:
            outname += "_mq"
        outname += ".pdf"

    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    # Save as PDF
    save_path_pdf = outdir / outname
    plt.savefig(save_path_pdf, format="pdf", bbox_inches="tight", dpi=300)
    print(f"PDF plot saved in {save_path_pdf}")
    # Save as PNG
    save_path_png = outdir / outname.replace(".pdf", ".png")
    plt.savefig(save_path_png, format="png", bbox_inches="tight", dpi=300)
    print(f"PNG plot saved in {save_path_png}")
    plt.clf()


@task
def plot_memcached(
    ctx,
    cvm="snp",
    vhost=False,
    mq=False,
    outdir="plot",
    outname=None,
    size="medium",
    result_dir=None,
):
    if result_dir is not None:
        global BENCH_RESULT_DIR
        BENCH_RESULT_DIR = Path(result_dir)

    bare_metal = "baremetal"
    bare_metal_label = "Native"

    if cvm == "snp":
        vm = "amd"
        vm_label = "VM"
        cvm_label = "SNP"
    else:
        vm = "intel"
        vm_label = "vm"
        cvm_label = "td"

    def get_name(name, vhost=False, p="", mq=mq):
        n = f"{name}-disk-{size}{p}"
        if vhost:
            n += "-vhost"
        if mq:
            n += "-mq"
        return n

    dfs = []
    dfs.append(
        parse_memtier_result(f"{bare_metal}-{size}", bare_metal_label, "memcached")
    )
    dfs.append(parse_memtier_result(get_name(vm), vm_label, "memcached"))
    dfs.append(
        parse_memtier_result(get_name(vm, vhost=True), f"{vm_label}-vhost", "memcached")
    )
    dfs.append(parse_memtier_result(get_name(cvm), cvm_label, "memcached"))
    dfs.append(
        parse_memtier_result(
            get_name(cvm, p="-haltpoll"), f"{cvm_label}-hpoll", "memcached"
        )
    )
    dfs.append(
        parse_memtier_result(get_name(cvm, p="-poll"), f"{cvm_label}-poll", "memcached")
    )
    dfs.append(
        parse_memtier_result(
            get_name(cvm, vhost=True), f"{cvm_label}-vhost", "memcached"
        )
    )
    dfs.append(
        parse_memtier_result(
            get_name(cvm, vhost=True, p="-haltpoll"),
            f"{cvm_label}-hpoll-vhost",
            "memcached",
        )
    )
    dfs.append(
        parse_memtier_result(
            get_name(cvm, vhost=True, p="-poll"), f"{cvm_label}-poll-vhost", "memcached"
        )
    )
    df = pd.concat(dfs)
    print(df)

    fig, ax = plt.subplots(figsize=(figwidth_half, 1.4))
    sns.barplot(
        x="workload",
        y="throughput",
        hue="name",
        data=df,
        ax=ax,
        palette=palette,
        edgecolor="black",
        err_kws={"linewidth": 0.6},
    )
    ax.set_xlabel("", fontsize=LABEL_FONTSIZE)
    ax.set_ylabel("Throughput (M req/s)", fontsize=LABEL_FONTSIZE)
    ax.set_title("Higher is better ↑", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    ax.tick_params(
        axis="x", labelsize=TICK_FONTSIZE, length=3, pad=1
    )  # Remove x-axis tick bars
    ax.tick_params(axis="y", labelsize=TICK_FONTSIZE, pad=2)

    # remove legend title
    ax.get_legend().set_title("")

    # set legend ncol
    ax.legend(loc="center", ncol=2, fontsize=LEGEND_FONTSIZE)
    ax.grid(True, alpha=0.3, axis="y")

    # annotate values with .2f
    for container in ax.containers:
        ax.bar_label(
            container, fmt="%.2f", fontsize=ANNOTATION_FONTSIZE, rotation=90, padding=2
        )

    # sns.despine(top=True)
    plt.tight_layout()

    if outname is None:
        outname = f"memcached"
        if vhost:
            outname += "_vhost"
        if mq:
            outname += "_mq"
        outname += ".pdf"

    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    # Save as PDF
    save_path_pdf = outdir / outname
    plt.savefig(save_path_pdf, format="pdf", bbox_inches="tight", dpi=300)
    print(f"PDF plot saved in {save_path_pdf}")
    # Save as PNG
    save_path_png = outdir / outname.replace(".pdf", ".png")
    plt.savefig(save_path_png, format="png", bbox_inches="tight", dpi=300)
    print(f"PNG plot saved in {save_path_png}")
    plt.clf()


def format_value(value):
    """Format value to remove unnecessary decimal places."""
    if value == int(value):
        return f"{int(value)}"  # Return as integer if it's a whole number
    else:
        return f"{value:.2f}".rstrip("0").rstrip(".")  # Remove trailing zeros


@task
def plot_network(
    ctx,
    cvm="snp",
    mode="tcp",
    vhost=False,
    mq=False,
    tmebypass=False,
    poll=False,
    outdir="plot",
    outname=None,
    size="medium",
    pkt=None,
    result_dir=None,
):
    """Create a combined plot with iperf and redis subplots side by side"""
    if result_dir is not None:
        global BENCH_RESULT_DIR
        BENCH_RESULT_DIR = Path(result_dir)

    bare_metal = "baremetal"
    bare_metal_label = "Native"

    pvm = ""
    pcvm = ""
    if cvm == "snp":
        vm = "amd"
        vm_label = "VM"
        cvm_label = "SNP"
    else:
        vm = "intel"
        vm_label = "vm"
        cvm_label = "td"
        if tmebypass:
            pvm = "-tmebypass"
    if poll:
        pvm += "-poll"
        pcvm += "-poll"

    def get_name(name, p, vhost=False, mq=mq, swiotlb=False):
        n = f"{name}-disk-{size}{p}"
        if vhost:
            n += "-vhost"
        if mq:
            n += "-mq"
        if swiotlb:
            n += "-swiotlb"
        return n

    # Parse iperf data
    iperf_dfs = []
    iperf_dfs.append(
        parse_iperf_result(f"{bare_metal}-{size}", bare_metal_label, mode, pkt=pkt)
    )
    iperf_dfs.append(parse_iperf_result(get_name(vm, pvm), vm_label, mode, pkt=pkt))
    # iperf_dfs.append(
    #     parse_iperf_result(
    #         get_name(vm, pvm, vhost=False, swiotlb=True),
    #         f"{vm_label}-swiotlb",
    #         mode,
    #         pkt=pkt,
    #     )
    # )
    iperf_dfs.append(
        parse_iperf_result(
            get_name(vm, pvm, vhost=True), f"{vm_label}-vhost", mode, pkt=pkt
        )
    )
    # iperf_dfs.append(
    #     parse_iperf_result(
    #         get_name(vm, pvm, vhost=True, swiotlb=True),
    #         f"{vm_label}-vhost-swiotlb",
    #         mode,
    #         pkt=pkt,
    #     )
    # )
    iperf_dfs.append(parse_iperf_result(get_name(cvm, pcvm), cvm_label, mode, pkt=pkt))
    # iperf_dfs.append(
    #     parse_iperf_result(
    #         get_name(cvm, "-haltpoll"), f"{cvm_label}-hpoll", mode, pkt=pkt
    #     )
    # )
    # iperf_dfs.append(
    #     parse_iperf_result(get_name(cvm, "-poll"), f"{cvm_label}-poll", mode, pkt=pkt)
    # )
    iperf_dfs.append(
        parse_iperf_result(
            get_name(cvm, pcvm, vhost=True), f"{cvm_label}-vhost", mode, pkt=pkt
        )
    )
    # iperf_dfs.append(
    #     parse_iperf_result(
    #         get_name(cvm, "-haltpoll-vhost"),
    #         f"{cvm_label}-vhost-hpoll",
    #         mode,
    #         pkt=pkt,
    #     )
    # )
    # iperf_dfs.append(
    #     parse_iperf_result(
    #         get_name(cvm, "-poll-vhost"), f"{cvm_label}-vhost-poll", mode, pkt=pkt
    #     )
    # )
    iperf_df = pd.concat(iperf_dfs)

    # Parse redis data
    redis_dfs = []
    redis_dfs.append(
        parse_memtier_result(f"{bare_metal}-{size}", bare_metal_label, "redis")
    )
    redis_dfs.append(parse_memtier_result(get_name(vm, ""), vm_label, "redis"))
    # redis_dfs.append(
    #     parse_memtier_result(
    #         get_name(vm, pvm, vhost=False, swiotlb=True), f"{vm_label}-swiotlb", "redis"
    #     )
    # )
    redis_dfs.append(
        parse_memtier_result(get_name(vm, "", vhost=True), f"{vm_label}-vhost", "redis")
    )
    # redis_dfs.append(
    #     parse_memtier_result(
    #         get_name(vm, pvm, vhost=True, swiotlb=True),
    #         f"{vm_label}-vhost-swiotlb",
    #         "redis",
    #     )
    # )
    redis_dfs.append(parse_memtier_result(get_name(cvm, ""), cvm_label, "redis"))
    # redis_dfs.append(
    #     parse_memtier_result(get_name(cvm, "-haltpoll"), f"{cvm_label}-hpoll", "redis")
    # )
    # redis_dfs.append(
    #     parse_memtier_result(get_name(cvm, "-poll"), f"{cvm_label}-poll", "redis")
    # )
    redis_dfs.append(
        parse_memtier_result(
            get_name(cvm, "", vhost=True), f"{cvm_label}-vhost", "redis"
        )
    )
    # redis_dfs.append(
    #     parse_memtier_result(
    #         get_name(cvm, "-haltpoll", vhost=True),
    #         f"{cvm_label}-vhost-hpoll",
    #         "redis",
    #     )
    # )
    # redis_dfs.append(
    #     parse_memtier_result(
    #         get_name(cvm, "-poll", vhost=True), f"{cvm_label}-vhost-poll", "redis"
    #     )
    # )
    redis_df = pd.concat(redis_dfs)

    print("Iperf data:")
    print(iperf_df)
    print("\nRedis data:")
    print(redis_df)

    # Create combined figure with two subplots
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(figwidth_half, 1.4))

    # Iperf subplot (left)
    unique_names = iperf_df["name"].unique()
    colors_for_plot = [
        LABEL_COLORS.get(name, palette[i % len(palette)])
        for i, name in enumerate(unique_names)
    ]
    sns.barplot(
        x="size",
        y="throughput",
        hue="name",
        data=iperf_df,
        ax=ax1,
        palette=colors_for_plot,
        edgecolor="black",
        err_kws={"linewidth": 0.6},
        linewidth=0.6,
    )

    if pkt is not None:
        ax1.set_xticklabels([], fontsize=TICK_FONTSIZE)
        ax1.set_xlabel("", fontsize=LABEL_FONTSIZE, labelpad=0)
    else:
        ax1.set_xlabel("Packet Size (byte)", fontsize=LABEL_FONTSIZE, labelpad=1)

    ax1.set_ylabel("Throughput (Gbps)", fontsize=LABEL_FONTSIZE, labelpad=0)
    ax1.set_title(
        "(a) Iperf (Higher is better ↑)", fontsize=FONTSIZE, color="navy", pad=3
    )
    ax1.tick_params(axis="x", labelsize=TICK_FONTSIZE, length=3, pad=1)
    ax1.tick_params(axis="y", labelsize=TICK_FONTSIZE, pad=0)
    ax1.get_legend().remove()  # Remove individual legend
    ax1.grid(True, alpha=0.3, axis="y")
    ylim1 = ax1.get_ylim()
    ax1.set_ylim(ylim1[0], ylim1[1] * 1.20)  # Add 20% headroom at top

    # Add annotations for iperf
    for container in ax1.containers:
        labels = [format_value(v.get_height()) for v in container]
        ax1.bar_label(
            container,
            labels=labels,
            rotation=90,
            padding=3,
            fontsize=ANNOTATION_FONTSIZE,
        )

    # Redis subplot (right)
    unique_names = redis_df["name"].unique()
    colors_for_plot = [
        LABEL_COLORS.get(name, palette[i % len(palette)])
        for i, name in enumerate(unique_names)
    ]
    sns.barplot(
        x="workload",
        y="throughput",
        hue="name",
        data=redis_df,
        ax=ax2,
        palette=colors_for_plot,
        edgecolor="black",
        err_kws={"linewidth": 0.6},
        linewidth=0.6,
    )

    ax2.set_xlabel("Workload", fontsize=LABEL_FONTSIZE, labelpad=1)
    ax2.set_ylabel("Throughput (M req/s)", fontsize=LABEL_FONTSIZE, labelpad=0)
    ax2.set_title(
        "(b) Redis (Higher is better ↑)", fontsize=FONTSIZE, color="navy", pad=3
    )
    ax2.tick_params(axis="x", labelsize=TICK_FONTSIZE, length=3, pad=1)
    ax2.tick_params(axis="y", labelsize=TICK_FONTSIZE, pad=0)
    ax2.get_legend().remove()  # Remove individual legend
    ax2.grid(True, alpha=0.3, axis="y")
    ylim2 = ax2.get_ylim()
    ax2.set_ylim(ylim2[0], ylim2[1] * 1.22)  # Add 20% headroom at top

    # Add annotations for redis
    for container in ax2.containers:
        labels = [format_value(v.get_height()) for v in container]
        ax2.bar_label(
            container,
            labels=labels,
            rotation=90,
            padding=2,
            fontsize=ANNOTATION_FONTSIZE,
        )

    # Create unified legend
    handles, labels = ax1.get_legend_handles_labels()
    if handles:
        fig.legend(
            handles,
            labels,
            loc="upper center",
            ncol=min(len(labels), 5),  # Limit columns to prevent overcrowding
            bbox_to_anchor=(0.5, 1.12),
            frameon=True,
            fontsize=LEGEND_FONTSIZE,
            columnspacing=1.5,
        )

    # Remove top spines
    # sns.despine(top=True, ax=ax1)
    # sns.despine(top=True, ax=ax2)

    # Layout adjustments
    plt.tight_layout()

    # Generate output filename
    if outname is None:
        outname = f"network_combined_{mode}"
        if vhost:
            outname += "_vhost"
        if mq:
            outname += "_mq"
        if pkt is not None:
            outname += f"_{pkt}"
        outname += f"{pvm}"
        outname += ".pdf"

    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    # Save as PDF
    save_path_pdf = outdir / outname
    plt.savefig(save_path_pdf, format="pdf", pad_inches=0, bbox_inches="tight", dpi=300)
    print(f"Combined PDF plot saved in {save_path_pdf}")

    # Save as PNG
    save_path_png = outdir / outname.replace(".pdf", ".png")
    plt.savefig(save_path_png, format="png", pad_inches=0, bbox_inches="tight", dpi=300)
    print(f"Combined PNG plot saved in {save_path_png}")

    plt.clf()
