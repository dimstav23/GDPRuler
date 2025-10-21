import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import plotly.graph_objects as go
import plotly.colors as pc
from pathlib import Path
from datetime import datetime
import argparse


outdir = "./plots"


def discover_latest_files(root_dir, workload_type, virtualization):
    """
    Discovers the latest CPU files for each variant.

    Returns:
        dict: Dictionary mapping variant names to their latest CPU file paths
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
                # Check if cpu.csv exists in this directory
                cpu_file = os.path.join(item_path, f"{virtualization}/cpu.csv")
                if os.path.exists(cpu_file):
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
                            datetime_dirs.append((parsed_datetime, cpu_file))
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


def process_cpu_data(cpu_file):
    """
    Processes a CPU file and returns the aggregate CPU utilization data.

    Returns:
        pd.DataFrame: DataFrame with time samples and CPU utilization
    """
    header_cols = [
        "hostname",
        "interval",
        "timestamp",
        "CPU",
        "%usr",
        "%nice",
        "%sys",
        "%iowait",
        "%steal",
        "%irq",
        "%soft",
        "%guest",
        "%gnice",
        "%idle",
    ]

    # Read the CSV file with explicit column names
    df = pd.read_csv(cpu_file, comment="#", names=header_cols)

    # Filter aggregate data where CPU = -1
    df_aggregate = df[df["CPU"] == -1].copy()

    if df_aggregate.empty:
        return None

    # Order artificially by row numbers (reset index for clear ordering)
    df_aggregate = df_aggregate.reset_index(drop=True)

    # Calculate total utilization (100 - idle)
    df_aggregate["total_util"] = 100 - df_aggregate["%idle"]

    return df_aggregate


def create_matplotlib_plot(latest_files, workload_type, virtualization):
    """
    Creates and saves a static matplotlib plot.
    """
    if not latest_files:
        print(f"No CPU files found for workload type: {workload_type}!")
        return

    # Set non-GUI backend for server compatibility
    mpl.use("Agg")

    # Create the plot
    plt.figure(figsize=(15, 8))

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
    for i, (variant, cpu_file) in enumerate(latest_files.items()):
        try:
            df_aggregate = process_cpu_data(cpu_file)
            if df_aggregate is None:
                print(f"No aggregate CPU data found for {variant}")
                continue

            # Plot using distinct colors
            color = distinct_colors[i % len(distinct_colors)]
            plt.plot(
                df_aggregate.index,
                df_aggregate["total_util"],
                label=variant,
                linewidth=2,
                marker="o",
                markersize=4,
                color=color,
            )

        except Exception as e:
            print(f"Error processing {variant}: {e}")
            continue

    # Customize the plot
    plt.xlabel("Time sample", fontsize=12)
    plt.ylabel("CPU Utilization (%)", fontsize=12)
    plt.title(
        f"CPU Utilization Timeline by Variant - {workload_type} - {virtualization.capitalize()} (Latest Results)",
        fontsize=14,
    )
    plt.legend(bbox_to_anchor=(1.05, 1), loc="upper left")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    # Save the static plot
    pathlib_outdir = Path(outdir)
    pathlib_outdir.mkdir(parents=True, exist_ok=True)

    output_file = (
        f"{outdir}/cpu_utilization_timeline_{workload_type}_{virtualization}.png"
    )
    plt.savefig(output_file, dpi=300, bbox_inches="tight")
    print(f"Static plot saved as: {output_file}")

    plt.close()  # Free memory


def create_plotly_plot(latest_files, workload_type, virtualization):
    """
    Creates and saves an interactive Plotly plot.
    """
    if not latest_files:
        print(f"No CPU files found for workload type: {workload_type}!")
        return

    # Create Plotly figure
    fig = go.Figure()

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
    for i, (variant, cpu_file) in enumerate(latest_files.items()):
        try:
            df_aggregate = process_cpu_data(cpu_file)
            if df_aggregate is None:
                print(f"No aggregate CPU data found for {variant}")
                continue

            # Add trace to Plotly figure
            fig.add_trace(
                go.Scatter(
                    x=df_aggregate.index,
                    y=df_aggregate["total_util"],
                    mode="lines+markers",
                    name=variant,
                    line=dict(color=colors[i % len(colors)], width=2),
                    marker=dict(size=4),
                    hovertemplate="<b>{}</b><br>"
                    + "Time Sample: %{{x}}<br>"
                    + "CPU Utilization: %{{y:.2f}}%<br>"
                    + "<extra></extra>".format(variant),
                )
            )

        except Exception as e:
            print(f"Error processing {variant}: {e}")
            continue

    # Update layout for better appearance
    fig.update_layout(
        title=dict(
            text=f"CPU Utilization Timeline by Variant - {workload_type} - {virtualization.capitalize()} (Latest Results)",
            x=0.5,
            font=dict(size=16),
        ),
        xaxis_title="Time Sample",
        yaxis_title="CPU Utilization (%)",
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
        width=1200,
        height=600,
        template="plotly_white",
        font=dict(size=12),
    )

    # Add grid and customize axes
    fig.update_xaxes(
        showgrid=True,
        gridwidth=1,
        gridcolor="rgba(128,128,128,0.2)",
        showline=True,
        linewidth=1,
        linecolor="black",
    )
    fig.update_yaxes(
        showgrid=True,
        gridwidth=1,
        gridcolor="rgba(128,128,128,0.2)",
        showline=True,
        linewidth=1,
        linecolor="black",
        range=[0, 100],
    )

    # Create output directory and save
    pathlib_outdir = Path(outdir)
    pathlib_outdir.mkdir(parents=True, exist_ok=True)

    html_file_path = (
        f"{outdir}/cpu_utilization_timeline_{workload_type}_{virtualization}.html"
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
                "filename": f"cpu_utilization_{workload_type}_{virtualization}",
                "height": 600,
                "width": 1200,
                "scale": 2,
            },
        },
    )
    print(f"Interactive HTML saved as: {html_file_path}")

    return fig


def create_cpu_utilization_plots(
    root_dir="../../trace-result/", virtualization="host", workload_type="redis"
):
    """
    Creates both matplotlib and Plotly plots for CPU utilization.
    """
    print(f"\n--- Processing {workload_type} {virtualization} ---")

    # Discover latest files
    latest_files = discover_latest_files(root_dir, workload_type, virtualization)

    if not latest_files:
        print(f"No CPU files found for workload type: {workload_type}!")
        return

    print(f"Found {len(latest_files)} variants")

    # Create both plots
    create_matplotlib_plot(latest_files, workload_type, virtualization)
    create_plotly_plot(latest_files, workload_type, virtualization)


def main():
    parser = argparse.ArgumentParser(
        description="Generate both static and interactive CPU utilization plots for different workload types"
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
    print("GENERATING BOTH STATIC AND INTERACTIVE CPU PLOTS")
    print("=" * 60)

    for workload_type in workload_types:
        print(f"\n=== {workload_type.upper()} WORKLOAD ===")
        # Generate plots for both host and guest
        for virtualization in ["host", "guest"]:
            create_cpu_utilization_plots(args.root_dir, virtualization, workload_type)

    print(f"\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    print(f"All plots saved in: {outdir}/")
    print("\nFiles generated:")
    print("📊 Static PNG plots (matplotlib):")
    print("   - cpu_utilization_timeline_<workload>_<host|guest>.png")
    print("🎯 Interactive HTML plots (Plotly):")
    print("   - cpu_utilization_timeline_<workload>_<host|guest>.html")

    print("\nInteractive features available in HTML files:")
    print("- Click legend items to show/hide variants")
    print("- Zoom by drawing a rectangle or using zoom controls")
    print("- Pan by dragging the plot")
    print("- Hover over points for detailed information")
    print("- Export plot as PNG using the camera icon")


if __name__ == "__main__":
    main()
