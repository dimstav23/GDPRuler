import pandas as pd
import matplotlib as mpl
import matplotlib.pyplot as plt
import seaborn as sns
import os
import re
import argparse
import numpy as np

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

FONTSIZE = 7
TITLE_FONTSIZE = FONTSIZE
LABEL_FONTSIZE = FONTSIZE
TICK_FONTSIZE = FONTSIZE - 1
LEGEND_FONTSIZE = FONTSIZE 

FONTSIZE = 6
TITLE_FONTSIZE = FONTSIZE
LABEL_FONTSIZE = FONTSIZE
TICK_FONTSIZE = FONTSIZE - 1
LEGEND_FONTSIZE = FONTSIZE
ANNOTATION_FONTSIZE = FONTSIZE / 2 - 1
# hatches = ["", "o", "*", ".", "//", "-", "\\", ".", "o-", "*-"]
hatches = ['', '///', '\\\\\\', 'xxx', '...', '+++', '', '///', '\\\\\\', 'xxx', '...', '+++']

workload_size = {
    "small": 1_000,
    "medium": 1_000_000,
    "large": 10_000_000
}

variant_mapping = {
    "direct_bare_metal"           : "Native DB",
    "passthrough_bare_metal"      : "Native passthrough",
    "gdpr_bare_metal"             : "Native GDPRuler",
    "direct_CVM"                  : "CVM DB",
    "passthrough_CVM"             : "CVM passthrough",
    "gdpr_CVM"                    : "CVM GDPRuler",
}

def load_logging_data(input_dir, variant="gdpr_CVM", n_clients=8, encryption="ON"):
    """Load and filter data for logging analysis."""
    pattern = r"(?P<controller>\w+(?:_\w+)?)-(?P<workload_type>[\w_]+)-encryption_(?P<encryption>\w+)-logging_(?P<logging>\w+)-connection_(?P<connection>\w+)\.csv"
    data = []

    for filename in os.listdir(input_dir):
        if filename.endswith(".csv"):
            match = re.match(pattern, filename)
            if match:
                # Filter for specific variant, encryption, and logging=ON
                if (match.group("controller") == variant and 
                    match.group("encryption") == encryption and
                    match.group("logging") == "ON"):

                    df = pd.read_csv(os.path.join(input_dir, filename))

                    # Filter for specific n_clients
                    df = df[df['n_clients'] == n_clients]

                    # Parse workload names to extract components
                    workload_pattern = r"(?P<workload_name>workload[a-z])_monitor_(?P<logged_percent>\d+)_(?P<size>\w+)"
                    # Extract components directly into separate columns
                    extracted = df['workload'].str.extract(workload_pattern)
                    df['workload_name'] = extracted['workload_name']
                    df['logged_percent'] = extracted['logged_percent'].astype(int)
                    df['workload_size'] = extracted['size']

                    # Drop rows where extraction failed
                    df = df.dropna(subset=['workload_name'])

                    # Calculate throughput (ops/s) and convert to kops
                    df['throughput_kops'] = (1_000_000 / df['elapsed_time (s)']) / 1000

                    # Add metadata
                    df['variant'] = match.group("controller")
                    df['encryption'] = match.group("encryption")
                    df['connection'] = match.group("connection")

                    data.append(df)

    if data:
        return pd.concat(data, ignore_index=True)
    else:
        return pd.DataFrame()

def prepare_plot_data_logging(df_subset, workload_name, logged_percent):
    """Prepare mean and std for plotting with error bars."""
    subset = df_subset[(df_subset['workload_name'] == workload_name) & 
                      (df_subset['logged_percent'] == logged_percent)]

    if subset.empty:
        return 0, 0

    mean_val = subset['throughput_kops'].mean()
    # Use standard deviation (population std with ddof=0, or sample std with ddof=1)
    std_val = subset['throughput_kops'].std(ddof=0)  # Use ddof=1 for sample std if preferred

    return mean_val, std_val

def create_logging_plot(data, variant, n_clients, encryption, output_dir):
    """Create side-by-side bar plots for Redis and RocksDB."""
    if data.empty:
        print(f"No data found for {variant}, n_clients={n_clients}, encryption={encryption}")
        return

    # Create figure with two subplots
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(figwidth_half, 1.5))

    # Get unique workload names and logged percentages
    workload_names = sorted(data['workload_name'].unique())
    logged_percentages = sorted(data['logged_percent'].unique())

    # Width of bars
    bar_width = 0.2
    x_pos = np.arange(len(workload_names))
    colors = sns.color_palette("pastel", len(logged_percentages))
    
    # Plot Redis data
    redis_data = data[data['db'] == 'redis']
    for i, percent in enumerate(logged_percentages):
        throughputs = []
        errors = []

        for workload in workload_names:
            mean_val, std_val = prepare_plot_data_logging(redis_data, workload, percent)
            throughputs.append(mean_val)
            errors.append(std_val)

        bar_positions = x_pos + i * bar_width
        bars = ax1.bar(bar_positions, throughputs, bar_width, 
                      label=f'{percent}%', color=colors[i], hatch=hatches[i % len(hatches)], 
                      alpha=0.8, edgecolor='black')

        # Add error bars
        ax1.errorbar(bar_positions, throughputs, yerr=errors,
                    fmt='none', ecolor='black', capsize=1, lw=0.5)
        
    ax1.set_xlabel('Workload', fontsize=LABEL_FONTSIZE, labelpad=2)
    ax1.set_ylabel('Throughput (kops/s)', fontsize=LABEL_FONTSIZE, labelpad=2)
    ax1.set_title(f'(a) Redis (Higher is better↑)', fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    ax1.set_xticks(x_pos + bar_width * (len(logged_percentages) - 1) / 2)
    ax1.set_xticklabels([w[-1].upper() for w in workload_names], fontsize=TICK_FONTSIZE)
    ax1.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
    ax1.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
    ax1.grid(True, alpha=0.3)

    # Plot RocksDB data
    rocksdb_data = data[data['db'] == 'rocksdb']
    for i, percent in enumerate(logged_percentages):
        throughputs = []
        errors = []

        for workload in workload_names:
            mean_val, std_val = prepare_plot_data_logging(rocksdb_data, workload, percent)
            throughputs.append(mean_val)
            errors.append(std_val)

        bar_positions = x_pos + i * bar_width
        bars = ax2.bar(bar_positions, throughputs, bar_width, 
                      label=f'{percent}%', color=colors[i], hatch=hatches[i % len(hatches)], 
                      alpha=0.8, edgecolor='black')

        # Add error bars
        ax2.errorbar(bar_positions, throughputs, yerr=errors,
                    fmt='none', ecolor='black', capsize=1, lw=0.5)

    ax2.set_xlabel('Workload', fontsize=LABEL_FONTSIZE, labelpad=2)
    ax2.set_ylabel('Throughput (kops/s)', fontsize=LABEL_FONTSIZE, labelpad=2)
    ax2.set_title(f'(b) RocksDB (Higher is better↑)', fontsize=TITLE_FONTSIZE, color="navy", pad=3)
    ax2.set_xticks(x_pos + bar_width * (len(logged_percentages) - 1) / 2)
    ax2.set_xticklabels([w[-1].upper() for w in workload_names], fontsize=TICK_FONTSIZE)
    ax2.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
    ax2.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
    ax2.grid(True, alpha=0.3)

    handles, labels = ax1.get_legend_handles_labels()

    fig.legend(handles, labels, title='Percentage of logged KV pairs', fontsize=LEGEND_FONTSIZE, 
               title_fontsize=LEGEND_FONTSIZE, loc='upper center', bbox_to_anchor=(0.55, 1.22), 
               ncol=len(logged_percentages))
    plt.tight_layout()

    # Save plot
    output_filename = f'logging_impact_{variant}_{n_clients}_clients_encryption_{encryption}'
    plt.savefig(os.path.join(output_dir, f'{output_filename}.png'), dpi=300, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_filename}.pdf'), bbox_inches='tight')
    plt.close()

    print(f"Plot saved: {output_filename}")

def print_storage_stats(data):
    """Print statistics about file storage and percentage differences."""
    if data.empty:
        print("No data available for statistics")
        return


    print("\n" + "="*80)
    print("STORAGE STATISTICS")
    print("="*80)


    # Group by database and workload
    for db in sorted(data['db'].unique()):
        db_data = data[data['db'] == db]
        print(f"\n{db.upper()} DATABASE:")
        print("-" * 40)


        for workload in sorted(db_data['workload_name'].unique()):
            workload_data = db_data[db_data['workload_name'] == workload]
            print(f"\n  {workload.upper()}:")


            # Get baseline (0% logging)
            baseline = workload_data[workload_data['logged_percent'] == 0]
            if baseline.empty:
                continue


            baseline_ctl_files = baseline['ctl_files_count'].mean()
            baseline_ctl_size = baseline['ctl_files_size_mb'].mean()
            baseline_db_files = baseline['db_files_count'].mean()
            baseline_db_size = baseline['db_files_size_mb'].mean()
            baseline_throughput = baseline['throughput_kops'].mean()


            print(f"    Baseline (0% logged):")
            print(f"      Controller files: {baseline_ctl_files:.0f} files, {baseline_ctl_size:.2f} MB")
            print(f"      DB files: {baseline_db_files:.0f} files, {baseline_db_size:.2f} MB")
            print(f"      Throughput: {baseline_throughput:.2f} kops/s")


            # Compare other percentages
            for percent in sorted(workload_data['logged_percent'].unique()):
                if percent == 0:
                    continue


                percent_data = workload_data[workload_data['logged_percent'] == percent]
                if percent_data.empty:
                    continue


                ctl_files = percent_data['ctl_files_count'].mean()
                ctl_size = percent_data['ctl_files_size_mb'].mean()
                db_files = percent_data['db_files_count'].mean()
                db_size = percent_data['db_files_size_mb'].mean()
                throughput = percent_data['throughput_kops'].mean()


                # Calculate percentage differences
                ctl_files_diff = ((ctl_files - baseline_ctl_files) / max(baseline_ctl_files, 1)) * 100
                ctl_size_diff = ((ctl_size - baseline_ctl_size) / max(baseline_ctl_size, 0.01)) * 100
                db_files_diff = ((db_files - baseline_db_files) / max(baseline_db_files, 1)) * 100
                db_size_diff = ((db_size - baseline_db_size) / max(baseline_db_size, 0.01)) * 100
                throughput_diff = ((throughput - baseline_throughput) / baseline_throughput) * 100


                print(f"\n    {percent}% logged:")
                print(f"      Controller files: {ctl_files:.0f} files ({ctl_files_diff:+.1f}%), {ctl_size:.2f} MB ({ctl_size_diff:+.1f}%)")
                print(f"      DB files: {db_files:.0f} files ({db_files_diff:+.1f}%), {db_size:.2f} MB ({db_size_diff:+.1f}%)")
                print(f"      Throughput: {throughput:.2f} kops/s ({throughput_diff:+.2f}%)")


def print_storage_amplification_table(data_dir):
    """Print storage amplification table for CVM variants."""

    # Load data from different experiment types
    def load_experiment_data(data_dir, controller_type, logging_state, encryption):
        pattern = r"(?P<controller>\w+(?:_\w+)?)-(?P<workload_type>[\w_]+)-encryption_(?P<encryption>\w+)-logging_(?P<logging>\w+)-connection_(?P<connection>\w+)\.csv"
        data = []

        for filename in os.listdir(data_dir):
            if filename.endswith(".csv"):
                match = re.match(pattern, filename)
                if match:
                    if (match.group("controller") == controller_type and 
                        match.group("logging") == logging_state and
                        match.group("encryption") == encryption):

                        df = pd.read_csv(os.path.join(data_dir, filename))
                        df['variant'] = match.group("controller")
                        df['encryption'] = match.group("encryption")
                        df['connection'] = match.group("connection")
                        data.append(df)

        if data:
            return pd.concat(data, ignore_index=True)
        else:
            return pd.DataFrame()

    # Load direct (vanilla) data - no GDPR metadata
    direct_data = load_experiment_data(data_dir, "direct_CVM", "OFF", "OFF")

    # Load GDPR data (logging OFF for baseline GDPR DB size)  
    gdpr_baseline = load_experiment_data(data_dir, "gdpr_CVM", "OFF", "ON")

    # Load GDPR logging data
    gdpr_logging = load_experiment_data(data_dir, "gdpr_CVM", "ON", "ON")

    # Parse logging data for different percentages
    if not gdpr_logging.empty:
        workload_pattern = r"(?P<workload_name>workload[a-z])_monitor_(?P<logged_percent>\d+)_(?P<size>\w+)"
        extracted = gdpr_logging['workload'].str.extract(workload_pattern)
        gdpr_logging['workload_name'] = extracted['workload_name']
        gdpr_logging['logged_percent'] = extracted['logged_percent'].astype(int)
        gdpr_logging['workload_size'] = extracted['size']
        gdpr_logging = gdpr_logging.dropna(subset=['workload_name'])

    # Calculate averages (only based workloadA and workloadC as these are the ones used in the monitor workloads)
    def get_db_size_average(data, db_type):
        if data.empty:
            return 0.0
        db_data = data[data['db'] == db_type]
        db_data = data[(data['db'] == db_type) & ((data['workload'] == "workloada_medium") | (data['workload'] == "workloadc_medium"))]
        return db_data['db_files_size_mb'].mean() if not db_data.empty else 0.0

    def get_gdpr_log_average(data, db_type, logged_percent, compression_level):
        if data.empty:
            return 0.0
        db_data = data[(data['db'] == db_type) & (data['logged_percent'] == logged_percent) & (data['compression_level'] == compression_level)]
        return db_data['ctl_files_size_mb'].mean() if not db_data.empty else 0.0

    # Get vanilla DB sizes from generic YCSB experiments based on WorkloadA and WorkloadC
    redis_vanilla = get_db_size_average(direct_data, 'redis')
    rocksdb_vanilla = get_db_size_average(direct_data, 'rocksdb')

    # Get GDPRuler DB sizes from generic YCSB experiments based on WorkloadA and WorkloadC
    redis_gdpr_db = get_db_size_average(gdpr_baseline, 'redis')
    rocksdb_gdpr_db = get_db_size_average(gdpr_baseline, 'rocksdb')

    # Get GDPR log sizes for different percentages and compression levels
    redis_logs_10_0 = get_gdpr_log_average(gdpr_logging, 'redis', 10, compression_level=0)
    redis_logs_50_0 = get_gdpr_log_average(gdpr_logging, 'redis', 50, compression_level=0)
    redis_logs_100_0 = get_gdpr_log_average(gdpr_logging, 'redis', 100, compression_level=0)

    rocksdb_logs_10_0 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 10, compression_level=0)
    rocksdb_logs_50_0 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 50, compression_level=0)
    rocksdb_logs_100_0 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 100, compression_level=0)
    
    redis_logs_10_3 = get_gdpr_log_average(gdpr_logging, 'redis', 10, compression_level=3)
    redis_logs_50_3 = get_gdpr_log_average(gdpr_logging, 'redis', 50, compression_level=3)
    redis_logs_100_3 = get_gdpr_log_average(gdpr_logging, 'redis', 100, compression_level=3)

    rocksdb_logs_10_3 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 10, compression_level=3)
    rocksdb_logs_50_3 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 50, compression_level=3)
    rocksdb_logs_100_3 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 100, compression_level=3)
    
    redis_logs_10_6 = get_gdpr_log_average(gdpr_logging, 'redis', 10, compression_level=6)
    redis_logs_50_6 = get_gdpr_log_average(gdpr_logging, 'redis', 50, compression_level=6)
    redis_logs_100_6 = get_gdpr_log_average(gdpr_logging, 'redis', 100, compression_level=6)

    rocksdb_logs_10_6 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 10, compression_level=6)
    rocksdb_logs_50_6 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 50, compression_level=6)
    rocksdb_logs_100_6 = get_gdpr_log_average(gdpr_logging, 'rocksdb', 100, compression_level=6)

    # Calculate write amplification (total data / vanilla DB size)
    def calc_write_amp(vanilla_size, gdpr_db_size, log_size):
        if vanilla_size == 0:
            return float('inf') if gdpr_db_size + log_size > 0 else 1.0
        return (gdpr_db_size + log_size) / vanilla_size

    redis_wa_10_comp_0 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_10_0)
    redis_wa_50_comp_0 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_50_0)
    redis_wa_100_comp_0 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_100_0)
    redis_wa_10_comp_3 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_10_3)
    redis_wa_50_comp_3 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_50_3)
    redis_wa_100_comp_3 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_100_3)
    redis_wa_10_comp_6 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_10_6)
    redis_wa_50_comp_6 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_50_6)
    redis_wa_100_comp_6 = calc_write_amp(redis_vanilla, redis_gdpr_db, redis_logs_100_6)

    rocksdb_wa_10_comp_0 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_10_0)
    rocksdb_wa_50_comp_0 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_50_0)
    rocksdb_wa_100_comp_0 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_100_0)
    rocksdb_wa_10_comp_3 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_10_3)
    rocksdb_wa_50_comp_3 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_50_3)
    rocksdb_wa_100_comp_3 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_100_3)
    rocksdb_wa_10_comp_6 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_10_6)
    rocksdb_wa_50_comp_6 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_50_6)
    rocksdb_wa_100_comp_6 = calc_write_amp(rocksdb_vanilla, rocksdb_gdpr_db, rocksdb_logs_100_6)

    # Print the table
    print("\n" + "="*90)
    print("STORAGE AMPLIFICATION ANALYSIS")
    print("="*90)
    print(f"{'Database':<12} {'Vanilla':<12} {'GDPRuler':<12} {'Compr.':<8} {'GDPR logs (MB)':<30}")
    print(f"{'':12} {'DB (MB)':<12} {'DB (MB)':<12} {'level':<8} {'10%':<8} {'50%':<10} {'100%':<12}")
    print("-" * 90)

    # Redis rows
    print(f"{'Redis':<12} {redis_vanilla:<12.2f} {redis_gdpr_db:<12.2f} {'0':<8} {redis_logs_10_0:<8.2f} {redis_logs_50_0:<10.2f} {redis_logs_100_0:<12.2f}")
    print(f"{'':12} {'':12} {'':12} {'3':<8} {redis_logs_10_3:<8.2f} {redis_logs_50_3:<10.2f} {redis_logs_100_3:<12.2f}")
    print(f"{'':12} {'':12} {'':12} {'6':<8} {redis_logs_10_6:<8.2f} {redis_logs_50_6:<10.2f} {redis_logs_100_6:<12.2f}")
    print()

    # RocksDB rows  
    print(f"{'RocksDB':<12} {rocksdb_vanilla:<12.2f} {rocksdb_gdpr_db:<12.2f} {'0':<8} {rocksdb_logs_10_0:<8.2f} {rocksdb_logs_50_0:<10.2f} {rocksdb_logs_100_0:<12.2f}")
    print(f"{'':12} {'':12} {'':12} {'3':<8} {rocksdb_logs_10_3:<8.2f} {rocksdb_logs_50_3:<10.2f} {rocksdb_logs_100_3:<12.2f}")
    print(f"{'':12} {'':12} {'':12} {'6':<8} {rocksdb_logs_10_6:<8.2f} {rocksdb_logs_50_6:<10.2f} {rocksdb_logs_100_6:<12.2f}")
    print()
    print("-" * 90)

    # Write amplification rows
    print(f"{'Write Amplification (Redis)':<38} {'0':<8} {redis_wa_10_comp_0:<8.2f} {redis_wa_50_comp_0:<10.2f} {redis_wa_100_comp_0:<12.2f}")
    print(f"{'':38} {'3':<8} {redis_wa_10_comp_3:<8.2f} {redis_wa_50_comp_3:<10.2f} {redis_wa_100_comp_3:<12.2f}")
    print(f"{'':38} {'6':<8} {redis_wa_10_comp_6:<8.2f} {redis_wa_50_comp_6:<10.2f} {redis_wa_100_comp_6:<12.2f}")
    print(f"{'Write Amplification (Rocksdb)':<38} {'0':<8} {rocksdb_wa_10_comp_0:<8.2f} {rocksdb_wa_50_comp_0:<10.2f} {rocksdb_wa_100_comp_0:<12.2f}")
    print(f"{'':38} {'3':<8} {rocksdb_wa_10_comp_3:<8.2f} {rocksdb_wa_50_comp_3:<10.2f} {rocksdb_wa_100_comp_3:<12.2f}")
    print(f"{'':38} {'6':<8} {rocksdb_wa_10_comp_6:<8.2f} {rocksdb_wa_50_comp_6:<10.2f} {rocksdb_wa_100_comp_6:<12.2f}")

    print("="*90)
    print()
    print("DETAILED BREAKDOWN:")
    print(f"Redis - Vanilla DB: {redis_vanilla:.2f} MB")
    print(f"Redis - GDPRuler DB: {redis_gdpr_db:.2f} MB") 
    print(f"Redis - GDPR logs (compression = 0): 10%={redis_logs_10_0:.2f} MB, 50%={redis_logs_50_0:.2f} MB, 100%={redis_logs_100_0:.2f} MB")
    print(f"Redis - GDPR logs (compression = 3): 10%={redis_logs_10_3:.2f} MB, 50%={redis_logs_50_3:.2f} MB, 100%={redis_logs_100_3:.2f} MB")
    print(f"Redis - GDPR logs (compression = 6): 10%={redis_logs_10_6:.2f} MB, 50%={redis_logs_50_6:.2f} MB, 100%={redis_logs_100_6:.2f} MB")
    print()
    print(f"RocksDB - Vanilla DB: {rocksdb_vanilla:.2f} MB")
    print(f"RocksDB - GDPRuler DB: {rocksdb_gdpr_db:.2f} MB")
    print(f"RocksDB - GDPR logs (compression = 3): 10%={rocksdb_logs_10_0:.2f} MB, 50%={rocksdb_logs_50_0:.2f} MB, 100%={rocksdb_logs_100_0:.2f} MB")
    print(f"RocksDB - GDPR logs (compression = 3): 10%={rocksdb_logs_10_3:.2f} MB, 50%={rocksdb_logs_50_3:.2f} MB, 100%={rocksdb_logs_100_3:.2f} MB")
    print(f"RocksDB - GDPR logs (compression = 6): 10%={rocksdb_logs_10_6:.2f} MB, 50%={rocksdb_logs_50_6:.2f} MB, 100%={rocksdb_logs_100_6:.2f} MB")

def main():
    parser = argparse.ArgumentParser(description="Generate logging impact plots and statistics")
    parser.add_argument("--bare_metal_results", type=str, default="../bare_metal/results",
                       help="Directory containing bare metal result CSV files")
    parser.add_argument("--vm_results", type=str, default="../VM/results", 
                       help="Directory containing VM result CSV files")
    parser.add_argument("--output_dir", type=str, default="plots",
                       help="Directory to save plots")
    parser.add_argument("--variant", type=str, default="gdpr_CVM", 
                       choices=["gdpr_bare_metal", "gdpr_CVM"],
                       help="Variant to analyze")
    parser.add_argument("--n_clients", type=int, default=8,
                       help="Number of clients to filter for")
    parser.add_argument("--encryption", type=str, default="ON", 
                       choices=["ON", "OFF"],
                       help="Encryption setting to filter for")

    args = parser.parse_args()

    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)

    # Determine input directory based on variant
    if "bare_metal" in args.variant:
        input_dir = args.bare_metal_results
    else:
        input_dir = args.vm_results

    print(f"Loading data from: {input_dir}")
    print(f"Variant: {args.variant}")
    print(f"Clients: {args.n_clients}")
    print(f"Encryption: {args.encryption}")

    # Load data
    data = load_logging_data(input_dir, args.variant, args.n_clients, args.encryption)

    if data.empty:
        print("No data found matching the criteria!")
        return

    print(f"Loaded {len(data)} data points")

    # Create plot
    create_logging_plot(data, args.variant, args.n_clients, args.encryption, args.output_dir)

    # Print statistics
    print_storage_stats(data)

    # Print storage amplification table
    print_storage_amplification_table(input_dir)

if __name__ == "__main__":
    main()
