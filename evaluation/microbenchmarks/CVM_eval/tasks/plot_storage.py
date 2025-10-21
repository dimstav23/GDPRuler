#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import json
from pathlib import Path
from typing import Any, Dict, List, Union

from invoke import task
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

# common graph settings
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


def read_json(file):
    lines = []
    with open(file) as f:
        # ignore error massages if any
        # (skip line until "{" is found)
        for line in f:
            if line.strip() == "{":
                lines.append(line)
                break
            else:
                print(f"[warn] skipping line: {line.strip()}")
        for line in f:
            lines.append(line)
    data = json.loads("".join(lines))
    return data


def process_data(data, name):
    d = []
    for job in data["jobs"]:
        d.append(
            [
                name,
                job["jobname"],
                float(job["read"]["iops_mean"]),
                float(job["read"]["iops_stddev"]),
                float(job["read"]["bw_mean"]),
                float(job["read"]["bw_dev"]),
                float(job["read"]["lat_ns"]["mean"]),
                float(job["read"]["lat_ns"]["stddev"]),
                float(job["write"]["iops_mean"]),
                float(job["write"]["iops_stddev"]),
                float(job["write"]["bw_mean"]),
                float(job["write"]["bw_dev"]),
                float(job["write"]["lat_ns"]["mean"]),
                float(job["write"]["lat_ns"]["stddev"]),
            ]
        )
    columns = [
        "name",
        "jobname",
        "read_iops_mean",
        "read_iops_stddev",
        "read_bw_mean",
        "read_bw_dev",
        "read_lat_mean",
        "read_lat_dev",
        "write_iops_mean",
        "write_iops_stddev",
        "write_bw_mean",
        "write_bw_dev",
        "write_lat_mean",
        "write_lat_dev",
    ]
    df = pd.DataFrame(d, columns=columns)
    return df


BENCH_RESULT_DIR = Path("./bench-result/fio")


def read_result(
    name: str, label: str, jobname: str, date=None, max_num: int = 10
) -> pd.DataFrame:
    dates = []
    if date is None:
        # use the latest results
        # note: fio reports stddev
        dates = [
            Path(i).stem
            for i in sorted(os.listdir(BENCH_RESULT_DIR / name / jobname))[-max_num:]
        ]
    else:
        dates.append(date)

    dfs = []
    for date in dates:
        file = BENCH_RESULT_DIR / name / jobname / f"{date}.json"
        data = read_json(file)
        df = process_data(data, label)
        dfs.append(df)

    # merge df
    df = pd.concat(dfs)
    return df


def plot_bw(df, outdir, outname, legend=True):
    fig, ax = plt.subplots(figsize=(figwidth_full / 1.5, 2.5))  # Made wider for 4 bars

    # Handle all bandwidth jobs
    bw_jobs = ["bw read", "bw write", "bw randread", "bw randwrite"]
    bw_data = []

    for job in bw_jobs:
        job_df = df[df["jobname"] == job].reset_index()
        if job_df.empty:
            continue

        # Select median for each system
        names = job_df["name"].unique()
        for name in names:
            if "read" in job:
                ranks = job_df[job_df["name"] == name]["read_bw_mean"].rank(pct=True)
                metric = "read_bw_mean"
                error = "read_bw_dev"
            else:
                ranks = job_df[job_df["name"] == name]["write_bw_mean"].rank(pct=True)
                metric = "write_bw_mean"
                error = "write_bw_dev"

            close_to_median = abs(ranks - 0.5)
            idx = close_to_median.idxmin()
            row = job_df.loc[idx].copy()
            row["metric_value"] = row[metric]
            row["error_value"] = row[error]
            bw_data.append(row)

    if not bw_data:
        print("No bandwidth data found")
        return

    bw_df = pd.DataFrame(bw_data)

    # Create the plot
    ax = sns.barplot(
        data=bw_df,
        x="jobname",
        y="metric_value",
        hue="name",
        palette=palette,
        edgecolor="k",
        linewidth=1.0,
    )

    # Add error bars
    h, l = ax.get_legend_handles_labels()
    x_coords = [p.get_x() + 0.5 * p.get_width() for p in ax.patches]
    y_coords = [p.get_height() for p in ax.patches]
    n = len(l)  # number of systems

    for i, job in enumerate(bw_jobs):
        job_data = bw_df[bw_df["jobname"] == job]
        if not job_data.empty:
            start_idx = i * n
            end_idx = start_idx + len(job_data)
            ax.errorbar(
                x=x_coords[start_idx:end_idx],
                y=y_coords[start_idx:end_idx],
                yerr=job_data["error_value"],
                fmt="none",
                c="k",
            )

    if ax.get_legend():
        ax.get_legend().set_title("")

    # Put numbers on top of bars
    for i, p in enumerate(ax.patches):
        height = p.get_height()
        if height == 0.0:
            continue
        ax.text(
            x=p.get_x() + p.get_width() / 2.0,
            y=height + height * 0.05,
            s=f"{height/1_000_000:.2f}",
            ha="center",
        )

    ax.yaxis.set_major_formatter(
        mpl.ticker.FuncFormatter(lambda val, pos: f"{val/1_000_000:g}")
    )

    ax.set_xticklabels(
        ["Seq Read", "Seq Write", "Rand Read", "Rand Write"], fontsize=TICK_FONTSIZE
    )

    ax.grid(True, alpha=0.3, axis="y")

    sns.move_legend(
        ax,
        "lower center",
        ncol=5,
        title=None,
        frameon=True,
    )

    if not legend:
        plt.legend([], [], frameon=False)

    plt.ylabel("Bandwidth [GiB/s]")
    plt.xlabel("")
    plt.title("Higher is better ↑", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    # sns.despine(top=True)
    plt.tight_layout()

    # Save as PDF
    outfile_pdf = Path(outdir) / outname
    plt.savefig(outfile_pdf, format="pdf", pad_inches=0, bbox_inches="tight")
    print(f"PDF saved to {outfile_pdf}")
    # Save as PNG
    outfile_png = Path(outdir) / outname.replace(".pdf", ".png")
    plt.savefig(outfile_png, format="png", pad_inches=0, bbox_inches="tight", dpi=300)
    print(f"PNG saved to {outfile_png}")
    plt.clf()


def plot_iops(df, outdir, outname="", legend=True):
    fig, ax = plt.subplots(
        figsize=(figwidth_full / 1.2, 2.5)
    )  # Made wider for more bars

    # Handle all IOPS jobs including your existing mixed workloads
    iops_jobs = [
        ("iops read", "read_iops_mean", "read_iops_stddev"),
        ("iops write", "write_iops_mean", "write_iops_stddev"),
        ("iops randread", "read_iops_mean", "read_iops_stddev"),
        ("iops randwrite", "write_iops_mean", "write_iops_stddev"),
    ]

    iops_data = []

    for job_name, metric, error_metric in iops_jobs:
        job_df = df[df["jobname"] == job_name].reset_index()
        if job_df.empty:
            continue

        # Select median for each system
        names = job_df["name"].unique()
        for name in names:
            if metric == "combined":
                # For mixed workloads, combine read and write IOPS
                job_df["iops_mean"] = (
                    job_df["read_iops_mean"] + job_df["write_iops_mean"]
                )
                ranks = job_df[job_df["name"] == name]["iops_mean"].rank(pct=True)
                close_to_median = abs(ranks - 0.5)
                idx = close_to_median.idxmin()
                row = job_df.loc[idx].copy()
                row["metric_value"] = row["iops_mean"]
                row["error_value"] = 0  # No error bars for combined metrics
            else:
                ranks = job_df[job_df["name"] == name][metric].rank(pct=True)
                close_to_median = abs(ranks - 0.5)
                idx = close_to_median.idxmin()
                row = job_df.loc[idx].copy()
                row["metric_value"] = row[metric]
                row["error_value"] = row[error_metric] if error_metric else 0

            iops_data.append(row)

    if not iops_data:
        print("No IOPS data found")
        return

    iops_df = pd.DataFrame(iops_data)

    # Create the plot
    ax = sns.barplot(
        data=iops_df,
        x="jobname",
        y="metric_value",
        hue="name",
        palette=palette,
        edgecolor="k",
        linewidth=1.0,
    )

    # Add error bars where applicable
    h, l = ax.get_legend_handles_labels()
    x_coords = [p.get_x() + 0.5 * p.get_width() for p in ax.patches]
    y_coords = [p.get_height() for p in ax.patches]
    n = len(l)  # number of systems

    job_names = iops_df["jobname"].unique()
    for i, job in enumerate(job_names):
        job_data = iops_df[iops_df["jobname"] == job]
        if not job_data.empty and job_data["error_value"].sum() > 0:
            start_idx = i * n
            end_idx = start_idx + len(job_data)
            ax.errorbar(
                x=x_coords[start_idx:end_idx],
                y=y_coords[start_idx:end_idx],
                yerr=job_data["error_value"],
                fmt="none",
                c="k",
                elinewidth=0.8,
            )

    if ax.get_legend():
        ax.get_legend().set_title("")

    # Put numbers on top of bars
    for i, p in enumerate(ax.patches):
        height = p.get_height()
        if height == 0.0:
            continue
        ax.text(
            x=p.get_x() + p.get_width() / 2.0,
            y=height + height * 0.05,
            s=f"{height/1000:.0f}",
            ha="center",
        )

    ax.yaxis.set_major_formatter(
        mpl.ticker.FuncFormatter(lambda val, pos: f"{val/1000:g}")
    )

    # Set appropriate labels based on what jobs are present
    job_labels = []
    for job_name, _, _ in iops_jobs:
        if job_name in iops_df["jobname"].values:
            if job_name == "iops read":
                job_labels.append("Seq Read")
            elif job_name == "iops write":
                job_labels.append("Seq Write")
            elif job_name == "iops randread":
                job_labels.append("Rand Read")
            elif job_name == "iops randwrite":
                job_labels.append("Rand Write")

    ax.set_xticklabels(job_labels, fontsize=TICK_FONTSIZE)
    ax.grid(True, alpha=0.3, axis="y")

    sns.move_legend(
        ax,
        "lower center",
        ncol=5,
        title=None,
        frameon=True,
    )

    if not legend:
        plt.legend([], [], frameon=False)

    plt.ylabel("Throughput (K IOPS)")
    plt.xlabel("")
    plt.title("Higher is better ↑", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    sns.despine(top=True)
    plt.tight_layout()

    # Save as PDF
    outfile_pdf = Path(outdir) / outname
    plt.savefig(outfile_pdf, format="pdf", pad_inches=0, bbox_inches="tight")
    print(f"PDF saved to {outfile_pdf}")
    # Save as PNG
    outfile_png = Path(outdir) / outname.replace(".pdf", ".png")
    plt.savefig(outfile_png, format="png", pad_inches=0, bbox_inches="tight", dpi=300)
    print(f"PNG saved to {outfile_png}")
    plt.clf()


def plot_latency(df, outdir, outname, legend=True):
    fig, ax = plt.subplots(figsize=(figwidth_full / 1.5, 2.5))

    # Handle all latency jobs - updated to use "lat" prefix instead of "alat"
    lat_jobs = [
        ("lat read", "read_lat_mean", "read_lat_dev"),
        ("lat write", "write_lat_mean", "write_lat_dev"),
        ("lat randread", "read_lat_mean", "read_lat_dev"),
        ("lat randwrite", "write_lat_mean", "write_lat_dev"),
    ]

    lat_data = []

    for job_name, metric, error_metric in lat_jobs:
        job_df = df[df["jobname"] == job_name].reset_index()
        if job_df.empty:
            continue

        # Select median for each system
        names = job_df["name"].unique()
        for name in names:
            ranks = job_df[job_df["name"] == name][metric].rank(pct=True)
            close_to_median = abs(ranks - 0.5)
            idx = close_to_median.idxmin()
            row = job_df.loc[idx].copy()
            row["metric_value"] = row[metric]
            row["error_value"] = row[error_metric]
            lat_data.append(row)

    if not lat_data:
        print("No latency data found")
        return

    lat_df = pd.DataFrame(lat_data)

    # Create the plot
    ax = sns.barplot(
        data=lat_df,
        x="jobname",
        y="metric_value",
        hue="name",
        palette=palette,
        edgecolor="k",
        linewidth=1.0,
    )

    # Add error bars
    h, l = ax.get_legend_handles_labels()
    x_coords = [p.get_x() + 0.5 * p.get_width() for p in ax.patches]
    y_coords = [p.get_height() for p in ax.patches]
    n = len(l)  # number of systems

    job_names = lat_df["jobname"].unique()
    for i, job in enumerate(job_names):
        job_data = lat_df[lat_df["jobname"] == job]
        if not job_data.empty:
            start_idx = i * n
            end_idx = start_idx + len(job_data)
            ax.errorbar(
                x=x_coords[start_idx:end_idx],
                y=y_coords[start_idx:end_idx],
                yerr=job_data["error_value"],
                fmt="none",
                c="k",
            )

    if ax.get_legend():
        ax.get_legend().set_title("")

    # Put numbers on top of bars
    for i, p in enumerate(ax.patches):
        height = p.get_height()
        if height == 0.0:
            continue
        ax.text(
            x=p.get_x() + p.get_width() / 2.0,
            y=height + height * 0.05,
            s=f"{height/1000:.0f}",
            ha="center",
        )

    ax.yaxis.set_major_formatter(
        mpl.ticker.FuncFormatter(lambda val, pos: f"{val/1000:g}")
    )

    ax.set_xticklabels(
        ["Seq Read", "Seq Write", "Rand Read", "Rand Write"], fontsize=TICK_FONTSIZE
    )

    ax.grid(True, alpha=0.3, axis="y")

    sns.move_legend(
        ax,
        "upper center",
        ncol=3,
        title=None,
        frameon=True,
    )

    if not legend:
        plt.legend([], [], frameon=False)

    plt.ylabel("4KB Latency (us)")
    plt.xlabel("")
    plt.title("Lower is better ↓", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    # sns.despine(top=True)
    plt.tight_layout()

    # Save as PDF
    outfile_pdf = Path(outdir) / outname
    plt.savefig(outfile_pdf, format="pdf", pad_inches=0, bbox_inches="tight")
    print(f"PDF saved to {outfile_pdf}")
    # Save as PNG
    outfile_png = Path(outdir) / outname.replace(".pdf", ".png")
    plt.savefig(outfile_png, format="png", pad_inches=0, bbox_inches="tight", dpi=300)
    print(f"PNG saved to {outfile_png}")
    plt.clf()


def plot_throughput_latency_combined(df, outdir, outname, legend=True):
    """Create side-by-side subplots for throughput (IOPS) and latency"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(figwidth_half, 1.4))

    # Throughput subplot (left)
    iops_jobs = [
        ("iops read", "read_iops_mean", "read_iops_stddev"),
        ("iops write", "write_iops_mean", "write_iops_stddev"),
        ("iops randread", "read_iops_mean", "read_iops_stddev"),
        ("iops randwrite", "write_iops_mean", "write_iops_stddev"),
    ]

    iops_data = []
    for job_name, metric, error_metric in iops_jobs:
        job_df = df[df["jobname"] == job_name].reset_index()
        if job_df.empty:
            continue

        names = job_df["name"].unique()
        for name in names:
            ranks = job_df[job_df["name"] == name][metric].rank(pct=True)
            close_to_median = abs(ranks - 0.5)
            idx = close_to_median.idxmin()
            row = job_df.loc[idx].copy()
            row["metric_value"] = row[metric]
            row["error_value"] = row[error_metric] if error_metric else 0
            iops_data.append(row)

    if iops_data:
        iops_df = pd.DataFrame(iops_data)

        unique_names = df["name"].unique()
        colors_for_plot = [
            LABEL_COLORS.get(name, palette[i % len(palette)])
            for i, name in enumerate(unique_names)
        ]
        sns.barplot(
            data=iops_df,
            x="jobname",
            y="metric_value",
            hue="name",
            palette=colors_for_plot,
            edgecolor="black",
            linewidth=0.6,
            err_kws={"linewidth": 0.6},
            ax=ax1,
        )

        # Add error bars for throughput
        h, l = ax1.get_legend_handles_labels()
        x_coords = [p.get_x() + 0.5 * p.get_width() for p in ax1.patches]
        y_coords = [p.get_height() for p in ax1.patches]
        n = len(l)

        # Store error values for positioning annotations
        error_heights = []
        job_names = iops_df["jobname"].unique()
        for i, job in enumerate(job_names):
            job_data = iops_df[iops_df["jobname"] == job]
            if not job_data.empty and job_data["error_value"].sum() > 0:
                start_idx = i * n
                end_idx = start_idx + len(job_data)
                ax1.errorbar(
                    x=x_coords[start_idx:end_idx],
                    y=y_coords[start_idx:end_idx],
                    yerr=job_data["error_value"],
                    fmt="none",
                    c="k",
                    elinewidth=0.6,
                )
                # Store error values for annotation positioning
                for val in job_data["error_value"]:
                    error_heights.append(val)
            else:
                for _ in range(len(job_data) if not job_data.empty else n):
                    error_heights.append(0)

        # Format throughput plot
        ax1.yaxis.set_major_formatter(
            mpl.ticker.FuncFormatter(lambda val, pos: f"{val/1000:g}")
        )
        ax1.set_xticklabels(
            ["Seq Read", "Seq Write", "Rand Read", "Rand Write"],
            fontsize=TICK_FONTSIZE,
            rotation=20,
        )
        ax1.tick_params(axis="x", length=3, pad=0)  # Remove x-axis tick bars
        ax1.tick_params(axis="y", labelsize=TICK_FONTSIZE, pad=0)
        ax1.set_ylabel("Throughput (K IOPS)", fontsize=LABEL_FONTSIZE, labelpad=0)
        ax1.set_xlabel("", fontsize=LABEL_FONTSIZE, labelpad=0)
        ax1.set_title(
            "(a) Throughput (Higher is better ↑)",
            fontsize=TITLE_FONTSIZE,
            color="navy",
            pad=3,
        )
        ax1.grid(True, alpha=0.3, axis="y")
        ylim1 = ax1.get_ylim()
        ax1.set_ylim(ylim1[0], ylim1[1] * 1.15)  # Add 15% headroom at top

        # Put numbers on top of bars for throughput
        for i, p in enumerate(ax1.patches):
            height = p.get_height()
            if height == 0.0:
                continue
            # Get the error value for this bar (if available)
            error_val = error_heights[i] if i < len(error_heights) else 0
            ax1.text(
                x=p.get_x() + p.get_width() / 1.5,
                y=height + error_val + height * 0.03,
                s=f"{height/1000:.0f}",
                ha="center",
                va="bottom",
                rotation=90,  # Vertical orientation
                fontsize=ANNOTATION_FONTSIZE,
            )

    # Latency subplot (right)
    lat_jobs = [
        ("lat read", "read_lat_mean", "read_lat_dev"),
        ("lat write", "write_lat_mean", "write_lat_dev"),
        ("lat randread", "read_lat_mean", "read_lat_dev"),
        ("lat randwrite", "write_lat_mean", "write_lat_dev"),
    ]

    lat_data = []
    for job_name, metric, error_metric in lat_jobs:
        job_df = df[df["jobname"] == job_name].reset_index()
        if job_df.empty:
            continue

        names = job_df["name"].unique()
        for name in names:
            ranks = job_df[job_df["name"] == name][metric].rank(pct=True)
            close_to_median = abs(ranks - 0.5)
            idx = close_to_median.idxmin()
            row = job_df.loc[idx].copy()
            row["metric_value"] = row[metric]
            row["error_value"] = row[error_metric]
            lat_data.append(row)

    if lat_data:
        lat_df = pd.DataFrame(lat_data)
        unique_names = df["name"].unique()
        colors_for_plot = [
            LABEL_COLORS.get(name, palette[i % len(palette)])
            for i, name in enumerate(unique_names)
        ]
        sns.barplot(
            data=lat_df,
            x="jobname",
            y="metric_value",
            hue="name",
            palette=colors_for_plot,
            edgecolor="k",
            linewidth=0.6,
            ax=ax2,
            legend=False,  # Remove legend from second plot to avoid duplication
        )

        # Add error bars for latency
        x_coords = [p.get_x() + 0.5 * p.get_width() for p in ax2.patches]
        y_coords = [p.get_height() for p in ax2.patches]

        # Store error values for positioning annotations
        error_heights_lat = []
        job_names = lat_df["jobname"].unique()
        for i, job in enumerate(job_names):
            job_data = lat_df[lat_df["jobname"] == job]
            if not job_data.empty:
                start_idx = i * n
                end_idx = start_idx + len(job_data)
                ax2.errorbar(
                    x=x_coords[start_idx:end_idx],
                    y=y_coords[start_idx:end_idx],
                    yerr=job_data["error_value"],
                    fmt="none",
                    c="k",
                    elinewidth=0.6,
                )
                # Store error values for annotation positioning
                for val in job_data["error_value"]:
                    error_heights_lat.append(val)
            else:
                for _ in range(len(job_data) if not job_data.empty else n):
                    error_heights_lat.append(0)

        # Format latency plot
        ax2.yaxis.set_major_formatter(
            mpl.ticker.FuncFormatter(lambda val, pos: f"{val/1000:g}")
        )
        ax2.set_xticklabels(
            ["Seq Read", "Seq Write", "Rand Read", "Rand Write"],
            fontsize=TICK_FONTSIZE,
            rotation=20,
        )
        ax2.tick_params(axis="x", length=3, pad=0)  # Remove x-axis tick bars
        ax2.tick_params(axis="y", labelsize=TICK_FONTSIZE, pad=0)
        ax2.set_ylabel("4KB Latency (us)", fontsize=LABEL_FONTSIZE, labelpad=0)
        ax2.set_xlabel("", fontsize=LABEL_FONTSIZE, labelpad=0)
        ax2.set_title(
            "(b) Latency (Lower is better ↓)",
            fontsize=TITLE_FONTSIZE,
            color="navy",
            pad=3,
        )
        ax2.grid(True, alpha=0.3, axis="y")
        ylim2 = ax2.get_ylim()
        ax2.set_ylim(ylim2[0], ylim2[1] * 1.15)  # Add 15% headroom at top

        # Put numbers on top of bars for latency
        for i, p in enumerate(ax2.patches):
            height = p.get_height()
            if height == 0.0:
                continue
            # Get the error value for this bar (if available)
            error_val = error_heights_lat[i] if i < len(error_heights_lat) else 0
            ax2.text(
                x=p.get_x() + p.get_width() / 1.5,
                y=height + error_val + height * 0.03,
                s=f"{height/1000:.0f}",
                ha="center",
                va="bottom",
                rotation=90,  # Vertical orientation
                fontsize=ANNOTATION_FONTSIZE,
            )

    # Remove any existing legends from axes
    if ax1.get_legend():
        ax1.get_legend().remove()
    if ax2.get_legend():
        ax2.get_legend().remove()

    # Set up legend using fig.legend() directly
    if legend and iops_data:
        # Get handles and labels from the first subplot
        handles, labels = ax1.get_legend_handles_labels()
        if handles:
            # Create figure-level legend
            fig.legend(
                handles,
                labels,
                loc="upper center",
                ncol=n,
                bbox_to_anchor=(0.5, 1.11),
                frameon=True,
                fontsize=LEGEND_FONTSIZE,
                columnspacing=1.5,
            )

    # Remove top spines
    # sns.despine(top=True, ax=ax1)
    # sns.despine(top=True, ax=ax2)
    # Print statistics for plotted values
    print("\n" + "=" * 80)
    print("STORAGE PERFORMANCE STATISTICS (Plotted Values)")
    print("=" * 80)

    # Throughput statistics
    if iops_data:
        print("\n--- THROUGHPUT (K IOPS) ---")
        for job_name in ["iops read", "iops write", "iops randread", "iops randwrite"]:
            job_label = job_name.replace("iops ", "").replace("rand", "Rand ").title()
            print(f"\n{job_label}:")

            job_data = iops_df[iops_df["jobname"] == job_name]
            if not job_data.empty:
                for name in job_data["name"].unique():
                    variant_data = job_data[job_data["name"] == name]
                    if not variant_data.empty:
                        throughput_kiops = variant_data["metric_value"].values[0] / 1000
                        error_kiops = variant_data["error_value"].values[0] / 1000
                        print(
                            f"  {name:20s}: {throughput_kiops:8.1f} ± {error_kiops:6.1f} K IOPS"
                        )

    # Latency statistics
    if lat_data:
        print("\n--- LATENCY (microseconds) ---")
        for job_name in ["lat read", "lat write", "lat randread", "lat randwrite"]:
            job_label = job_name.replace("lat ", "").replace("rand", "Rand ").title()
            print(f"\n{job_label}:")

            job_data = lat_df[lat_df["jobname"] == job_name]
            if not job_data.empty:
                for name in job_data["name"].unique():
                    variant_data = job_data[job_data["name"] == name]
                    if not variant_data.empty:
                        latency_us = variant_data["metric_value"].values[0] / 1000
                        error_us = variant_data["error_value"].values[0] / 1000
                        print(f"  {name:20s}: {latency_us:8.1f} ± {error_us:6.1f} us")

    # Performance comparison table
    if iops_data and lat_data:
        print("\n--- PERFORMANCE COMPARISON ---")
        print(
            f"{'Variant':<20} {'Seq Read':<15} {'Seq Write':<15} {'Rand Read':<15} {'Rand Write':<15}"
        )
        print(
            f"{'':20} {'(K IOPS)':<15} {'(K IOPS)':<15} {'(K IOPS)':<15} {'(K IOPS)':<15}"
        )
        print("-" * 80)

        for name in iops_df["name"].unique():
            values = []
            for job_name in [
                "iops read",
                "iops write",
                "iops randread",
                "iops randwrite",
            ]:
                job_data = iops_df[
                    (iops_df["jobname"] == job_name) & (iops_df["name"] == name)
                ]
                if not job_data.empty:
                    val = job_data["metric_value"].values[0] / 1000
                    values.append(f"{val:.1f}")
                else:
                    values.append("N/A")

            print(
                f"{name:<20} {values[0]:<15} {values[1]:<15} {values[2]:<15} {values[3]:<15}"
            )

        print("\n")
        print(
            f"{'Variant':<20} {'Seq Read':<15} {'Seq Write':<15} {'Rand Read':<15} {'Rand Write':<15}"
        )
        print(f"{'':20} {'(us)':<15} {'(us)':<15} {'(us)':<15} {'(us)':<15}")
        print("-" * 80)

        for name in lat_df["name"].unique():
            values = []
            for job_name in ["lat read", "lat write", "lat randread", "lat randwrite"]:
                job_data = lat_df[
                    (lat_df["jobname"] == job_name) & (lat_df["name"] == name)
                ]
                if not job_data.empty:
                    val = job_data["metric_value"].values[0] / 1000
                    values.append(f"{val:.1f}")
                else:
                    values.append("N/A")

            print(
                f"{name:<20} {values[0]:<15} {values[1]:<15} {values[2]:<15} {values[3]:<15}"
            )

    print("\n" + "=" * 80 + "\n")

    plt.tight_layout()

    # Save as PDF
    outfile_pdf = Path(outdir) / outname
    plt.savefig(outfile_pdf, format="pdf", pad_inches=0, bbox_inches="tight", dpi=300)
    print(f"Combined PDF saved to {outfile_pdf}")
    # Save as PNG
    outfile_png = Path(outdir) / outname.replace(".pdf", ".png")
    plt.savefig(outfile_png, format="png", pad_inches=0, bbox_inches="tight", dpi=300)
    print(f"Combined PNG saved to {outfile_png}")
    plt.clf()


@task
def plot_fio(
    ctx: Any,
    cvm="snp",
    size="medium",
    aio="native",
    jobfile="libaio",
    outdir="plot",
    device="nvme1n1",
    tmebypass=False,
    poll=False,
    all=False,
    swiotlb=True,
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

    dfs = []
    dfs.append(read_result(f"{bare_metal}-{size}", bare_metal_label, jobfile))
    dfs.append(read_result(f"{vm}-disk-{size}{pvm}-{aio}", vm_label, jobfile))
    # dfs.append(
    #     read_result(
    #         f"{vm}-disk-{size}-{aio}{pvm}-swiotlb", f"{vm_label}-swiotlb", jobfile
    #     )
    # )
    dfs.append(read_result(f"{cvm}-disk-{size}{pcvm}-{aio}", cvm_label, jobfile))
    dfs.append(
        read_result(f"{cvm}-disk-{size}-poll-{aio}", f"{cvm_label}-poll", jobfile)
    )
    # dfs.append(
    #     read_result(f"{cvm}-disk-{size}-haltpoll-{aio}", f"{cvm_label}-hpoll", jobfile)
    # )

    df = pd.concat(dfs)
    print(df)

    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    # save df
    df.to_csv(Path(outdir) / f"fio_{pvm}.csv", index=False)

    if all:
        pvm += "all"
    # plot_bw(df, outdir, f"fio_bw_{pvm}.pdf", legend=True)
    # plot_iops(df, outdir, f"fio_iops_{pvm}.pdf", legend=False)
    # plot_latency(df, outdir, f"fio_latency_{pvm}.pdf", legend=False)

    # Generate combined throughput and latency plot
    plot_throughput_latency_combined(
        df, outdir, f"fio_throughput_latency_{pvm}.pdf", legend=True
    )


@task
def analyze_fio(
    ctx: Any,
    cvm="snp",
    size="medium",
    aio="native",
    jobfile="libaio",
    outdir="plot",
    device="nvme1n1",
    tmebypass=False,
    poll=False,
    swiotlb=False,
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
        vm_label = "vm"
        cvm_label = "snp"
    else:
        vm = "intel"
        vm_label = "vm"
        cvm_label = "td"
        if tmebypass:
            pvm = "-tmebypass"
    if poll:
        pvm += "-poll"
        pcvm += "-poll"

    bdf = read_result(f"{bare_metal}-{size}", bare_metal_label, jobfile, max_num=10)
    df = read_result(f"{vm}-disk-{size}-{pvm}-{aio}", vm_label, jobfile, max_num=10)
    cdf = read_result(f"{cvm}-disk-{size}-{pcvm}-{aio}", cvm_label, jobfile, max_num=10)

    print(bdf[(bdf["jobname"] == "bw read")]["read_bw_mean"])
    print(df[(df["jobname"] == "bw read")]["read_bw_mean"])
    print(cdf[(cdf["jobname"] == "bw read")]["read_bw_mean"])

    print(bdf[(bdf["jobname"] == "bw write")]["write_bw_mean"])
    print(df[(df["jobname"] == "bw write")]["write_bw_mean"])
    print(cdf[(cdf["jobname"] == "bw write")]["write_bw_mean"])

    print(bdf[(bdf["jobname"] == "iops randread")]["read_iops_mean"])
    print(df[(df["jobname"] == "iops randread")]["read_iops_mean"])
    print(cdf[(cdf["jobname"] == "iops randread")]["read_iops_mean"])

    print(bdf[(bdf["jobname"] == "iops randwrite")]["write_iops_mean"])
    print(df[(df["jobname"] == "iops randwrite")]["write_iops_mean"])
    print(cdf[(cdf["jobname"] == "iops randwrite")]["write_iops_mean"])
