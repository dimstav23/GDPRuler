import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import plotly.colors as pc
from pathlib import Path
from datetime import datetime
import argparse


outdir = "./plots"


def discover_latest_memory_files(root_dir, workload_type, virtualization):
    """
    Discovers the latest memory files for each variant.

    Returns:
        dict: Dictionary mapping variant names to their latest memory file paths
    """
    latest_files = {}

    # Scan root directory for variant folders
    for variant in os.listdir(root_dir):
        # Filter variants based on workload type
        if not variant.endswith(f"_{workload_type}"):
            continue

        variant_path = os.path.join(root_dir, variant)

        # Skip if not a directory
        if not os.path.isdir(variant_path):
            continue

        # Find all datetime directories in this variant folder
        datetime_dirs = []

        for item in os.listdir(variant_path):
            item_path = os.path.join(variant_path, item)
            if os.path.isdir(item_path):
                # Check if memory.csv exists in this directory
                memory_file = os.path.join(item_path, f"{virtualization}/memory.csv")
                if os.path.exists(memory_file):
                    try:
                        # Try to parse datetime from directory name
                        possible_formats = [
                            "%Y-%m-%d-%H-%M-%S",
                            "%Y%m%d_%H%M%S",
                            "%Y-%m-%d_%H%M%S",
                            "%Y%m%d-%H%M%S",
                        ]

                        parsed_datetime = None
                        for fmt in possible_formats:
                            try:
                                parsed_datetime = datetime.strptime(item, fmt)
                                break
                            except ValueError:
                                continue

                        if parsed_datetime:
                            datetime_dirs.append((parsed_datetime, memory_file))
                    except:
                        continue

        # Get the latest datetime directory for this variant
        if datetime_dirs:
            latest_datetime, latest_file = max(datetime_dirs, key=lambda x: x[0])
            latest_files[variant] = latest_file
            print(
                f"Latest file for {variant} {virtualization}: {latest_file} ({latest_datetime})"
            )

    return latest_files


def process_memory_data(memory_file):
    """
    Processes a memory file and returns the processed data.

    Returns:
        pd.DataFrame: DataFrame with memory data converted to MB
    """
    header_cols = [
        "hostname",
        "interval",
        "timestamp",
        "kbmemfree",
        "kbavail",
        "kbmemused",
        "%memused",
        "kbbuffers",
        "kbcached",
        "kbcommit",
        "%commit",
        "kbactive",
        "kbinact",
        "kbdirty",
    ]

    # Read the CSV file with explicit column names
    df = pd.read_csv(memory_file, comment="#", names=header_cols)

    if df.empty:
        return None

    # Order by row numbers for precise ordering
    df = df.reset_index(drop=True)

    # Convert memory values from KB to MB for better readability
    df["mbmemused"] = df["kbmemused"] / 1024
    df["mbmemfree"] = df["kbmemfree"] / 1024
    df["mbcached"] = df["kbcached"] / 1024
    df["mbcommit"] = df["kbcommit"] / 1024

    return df


def create_matplotlib_memory_plot(latest_files, workload_type, virtualization):
    """
    Creates and saves a static matplotlib plot for memory data.
    """
    if not latest_files:
        print(f"No memory files found for workload type: {workload_type}!")
        return

    # Set non-GUI backend for server compatibility
    mpl.use("Agg")

    # Create subplots for different memory metrics
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(20, 12))

    # Define distinct colors
    distinct_colors = [
        "#1f77b4",
        "#ff7f0e",
        "#2ca02c",
        "#d62728",
        "#9467bd",
        "#8c564b",
        "#e377c2",
        "#7f7f7f",
        "#bcbd22",
        "#17becf",
        "#aec7e8",
        "#ffbb78",
        "#98df8a",
        "#ff9896",
        "#c5b0d5",
        "#c49c94",
        "#f7b6d3",
        "#c7c7c7",
        "#dbdb8d",
        "#9edae5",
        "#FF6B6B",
        "#4ECDC4",
        "#45B7D1",
        "#96CEB4",
        "#FFEAA7",
        "#DDA0DD",
        "#98D8C8",
        "#F7DC6F",
        "#BB8FCE",
        "#85C1E9",
        "#F8C471",
        "#82E0AA",
        "#F1948A",
        "#85C1E9",
        "#D7DBDD",
        "#A569BD",
        "#5DADE2",
        "#58D68D",
        "#F4D03F",
        "#EC7063",
    ]

    # Plot data for each variant
    for i, (variant, memory_file) in enumerate(latest_files.items()):
        try:
            df = process_memory_data(memory_file)
            if df is None or df.empty:
                print(f"No memory data found for {variant}")
                continue

            color = distinct_colors[i % len(distinct_colors)]

            # Plot memory used (MB)
            ax1.plot(
                df.index,
                df["mbmemused"],
                label=variant,
                linewidth=2,
                marker="o",
                markersize=4,
                color=color,
            )

            # Plot memory utilization percentage
            ax2.plot(
                df.index,
                df["%memused"],
                label=variant,
                linewidth=2,
                marker="s",
                markersize=4,
                color=color,
            )

            # Plot cached memory (MB)
            ax3.plot(
                df.index,
                df["mbcached"],
                label=variant,
                linewidth=2,
                marker="^",
                markersize=4,
                color=color,
            )

            # Plot commit percentage
            ax4.plot(
                df.index,
                df["%commit"],
                label=variant,
                linewidth=2,
                marker="d",
                markersize=4,
                color=color,
            )

        except Exception as e:
            print(f"Error processing {variant}: {e}")
            continue

    # Customize each subplot
    ax1.set_xlabel("Time Sample")
    ax1.set_ylabel("Memory Used (MB)")
    ax1.set_title("Memory Used Over Time")
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.set_xlabel("Time Sample")
    ax2.set_ylabel("Memory Used (%)")
    ax2.set_title("Memory Utilization Percentage Over Time")
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    ax3.set_xlabel("Time Sample")
    ax3.set_ylabel("Cached Memory (MB)")
    ax3.set_title("Cached Memory Over Time")
    ax3.legend()
    ax3.grid(True, alpha=0.3)

    ax4.set_xlabel("Time Sample")
    ax4.set_ylabel("Commit (%)")
    ax4.set_title("Memory Commit Percentage Over Time")
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    # Overall title
    fig.suptitle(
        f"Memory Consumption Timeline by Variant - {workload_type} - {virtualization.capitalize()} (Latest Results)",
        fontsize=16,
        y=0.98,
    )

    plt.tight_layout()

    # Save the static plot
    pathlib_outdir = Path(outdir)
    pathlib_outdir.mkdir(parents=True, exist_ok=True)

    output_file = (
        f"{outdir}/memory_consumption_timeline_{workload_type}_{virtualization}.png"
    )
    plt.savefig(output_file, dpi=300, bbox_inches="tight")
    print(f"Static plot saved as: {output_file}")

    plt.close()  # Free memory


def create_plotly_memory_plot(latest_files, workload_type, virtualization):
    """
    Creates and saves an interactive Plotly plot for memory data.
    """
    if not latest_files:
        print(f"No memory files found for workload type: {workload_type}!")
        return

    # Create subplots using Plotly
    fig = make_subplots(
        rows=2,
        cols=2,
        subplot_titles=(
            "Memory Used (MB)",
            "Memory Utilization (%)",
            "Cached Memory (MB)",
            "Memory Commit (%)",
        ),
        vertical_spacing=0.12,
        horizontal_spacing=0.1,
    )

    # Extended color palette for Plotly (40+ distinct colors)
    colors = (
        pc.qualitative.Dark24
        + pc.qualitative.Light24
        + pc.qualitative.Set1
        + pc.qualitative.Set2
        + pc.qualitative.Set3
        + pc.qualitative.Plotly
        + pc.qualitative.T10
        + pc.qualitative.Alphabet
    )
    # Remove any duplicates and ensure we have enough colors
    colors = list(dict.fromkeys(colors))  # Remove duplicates while preserving order

    # Plot data for each variant
    for i, (variant, memory_file) in enumerate(latest_files.items()):
        try:
            df = process_memory_data(memory_file)
            if df is None or df.empty:
                print(f"No memory data found for {variant}")
                continue

            color = colors[i % len(colors)]

            # Add traces to respective subplots
            # FIXED: Use .format() instead of f-strings for hovertemplate
            fig.add_trace(
                go.Scatter(
                    x=df.index,
                    y=df["mbmemused"],
                    mode="lines+markers",
                    name="{} Used".format(variant),
                    line=dict(color=color, width=2),
                    marker=dict(symbol="circle", size=6),
                    hovertemplate="<b>{} Used</b><br>Time Sample: %{{x}}<br>Memory (MB): %{{y:.2f}}<extra></extra>".format(
                        variant
                    ),
                    legendgroup=variant,
                ),
                row=1,
                col=1,
            )

            fig.add_trace(
                go.Scatter(
                    x=df.index,
                    y=df["%memused"],
                    mode="lines+markers",
                    name="{} Util %".format(variant),
                    line=dict(color=color, width=2),
                    marker=dict(symbol="square", size=6),
                    hovertemplate="<b>{} Util %</b><br>Time Sample: %{{x}}<br>Memory Utilization: %{{y:.2f}}%<extra></extra>".format(
                        variant
                    ),
                    legendgroup=variant,
                    showlegend=False,
                ),
                row=1,
                col=2,
            )

            fig.add_trace(
                go.Scatter(
                    x=df.index,
                    y=df["mbcached"],
                    mode="lines+markers",
                    name="{} Cached".format(variant),
                    line=dict(color=color, width=2),
                    marker=dict(symbol="triangle-up", size=6),
                    hovertemplate="<b>{} Cached</b><br>Time Sample: %{{x}}<br>Memory (MB): %{{y:.2f}}<extra></extra>".format(
                        variant
                    ),
                    legendgroup=variant,
                    showlegend=False,
                ),
                row=2,
                col=1,
            )

            fig.add_trace(
                go.Scatter(
                    x=df.index,
                    y=df["%commit"],
                    mode="lines+markers",
                    name="{} Commit %".format(variant),
                    line=dict(color=color, width=2),
                    marker=dict(symbol="diamond", size=6),
                    hovertemplate="<b>{} Commit %</b><br>Time Sample: %{{x}}<br>Memory Commit: %{{y:.2f}}%<extra></extra>".format(
                        variant
                    ),
                    legendgroup=variant,
                    showlegend=False,
                ),
                row=2,
                col=2,
            )

        except Exception as e:
            print(f"Error processing {variant}: {e}")
            continue

    # Update layout
    fig.update_layout(
        title=dict(
            text=f"Memory Consumption Timeline by Variant - {workload_type} - {virtualization.capitalize()} (Latest Results)",
            x=0.5,
            font=dict(size=16),
        ),
        hovermode="closest",
        legend=dict(
            orientation="v",
            yanchor="top",
            y=1,
            xanchor="left",
            x=1.02,
            bgcolor="rgba(255,255,255,0.8)",
            bordercolor="rgba(0,0,0,0.2)",
            borderwidth=1,
        ),
        width=1400,
        height=800,
        template="plotly_white",
        font=dict(size=12),
    )

    # Update x and y axes for all subplots
    fig.update_xaxes(
        title_text="Time Sample",
        showgrid=True,
        gridwidth=1,
        gridcolor="rgba(128,128,128,0.2)",
    )
    fig.update_yaxes(
        title_text="Memory",
        showgrid=True,
        gridwidth=1,
        gridcolor="rgba(128,128,128,0.2)",
    )

    # Create output directory and save
    pathlib_outdir = Path(outdir)
    pathlib_outdir.mkdir(parents=True, exist_ok=True)

    html_file_path = (
        f"{outdir}/memory_consumption_timeline_{workload_type}_{virtualization}.html"
    )
    fig.write_html(
        html_file_path,
        include_plotlyjs=True,
        config={
            "displayModeBar": True,
            "displaylogo": False,
            "modeBarButtonsToRemove": ["pan2d", "lasso2d", "select2d"],
            "toImageButtonOptions": {
                "format": "png",
                "filename": f"memory_consumption_timeline_{workload_type}_{virtualization}",
                "height": 800,
                "width": 1400,
                "scale": 2,
            },
        },
    )
    print(f"Interactive HTML saved as: {html_file_path}")

    return fig


def create_memory_consumption_timeline_plots(
    root_dir="../../trace-result/", virtualization="host", workload_type="redis"
):
    """
    Creates both matplotlib and Plotly plots for memory consumption data.
    """
    print(f"\n--- Processing {workload_type} {virtualization} ---")

    # Discover latest files
    latest_files = discover_latest_memory_files(root_dir, workload_type, virtualization)

    if not latest_files:
        print(f"No memory files found for workload type: {workload_type}!")
        return

    print(f"Found {len(latest_files)} variants")

    # Create both plots
    create_matplotlib_memory_plot(latest_files, workload_type, virtualization)
    create_plotly_memory_plot(latest_files, workload_type, virtualization)


def main():
    parser = argparse.ArgumentParser(
        description="Generate both static and interactive memory consumption plots for different workload types"
    )
    parser.add_argument(
        "--redis", action="store_true", help="Generate plots for Redis workload"
    )
    parser.add_argument(
        "--memcached", action="store_true", help="Generate plots for memcached workload"
    )
    parser.add_argument(
        "--fio", action="store_true", help="Generate plots for FIO workload"
    )
    parser.add_argument(
        "--network", action="store_true", help="Generate plots for Network workload"
    )
    parser.add_argument(
        "--root-dir",
        default="../../trace-result/",
        help="Root directory containing variant folders",
    )

    args = parser.parse_args()

    # Check if at least one workload type is specified
    if not (args.redis or args.fio or args.network or args.memcached):
        print(
            "Error: Please specify at least one workload type (--redis, --fio, --memcached or --network)"
        )
        parser.print_help()
        return

    # Create output directory
    outpath = Path(outdir)
    outpath.mkdir(parents=True, exist_ok=True)

    # Generate plots for each specified workload type
    workload_types = []
    if args.redis:
        workload_types.append("redis")
    if args.fio:
        workload_types.append("fio")
    if args.network:
        workload_types.append("network")
    if args.memcached:
        workload_types.append("memcached")

    print("=" * 60)
    print("GENERATING BOTH STATIC AND INTERACTIVE MEMORY CONSUMPTION PLOTS")
    print("=" * 60)

    for workload_type in workload_types:
        print(f"\n=== {workload_type.upper()} WORKLOAD ===")
        # Generate plots for both host and guest
        for virtualization in ["host", "guest"]:
            create_memory_consumption_timeline_plots(
                args.root_dir, virtualization, workload_type
            )

    print(f"\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    print(f"All plots saved in: {outdir}/")
    print("\nFiles generated:")
    print("📊 Static PNG plots (matplotlib):")
    print("   - memory_consumption_timeline_<workload>_<host|guest>.png")
    print("🎯 Interactive HTML plots (Plotly):")
    print("   - memory_consumption_timeline_<workload>_<host|guest>.html")

    print("\nInteractive features available in HTML files:")
    print("- Click legend items to show/hide variants")
    print("- Zoom by drawing a rectangle or using zoom controls")
    print("- Pan by dragging the plot")
    print("- Hover over points for detailed information")
    print("- Export plot as PNG using the camera icon")
    print(
        "- 4 subplots: Memory Used (MB), Memory Util (%), Cached Memory (MB), Commit (%)"
    )


if __name__ == "__main__":
    main()
