import pandas as pd
import matplotlib as mpl  # type: ignore
import matplotlib.pyplot as plt
import seaborn as sns
import os
import re
import argparse

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

def load_data_from_directory(input_dir):
    pattern = r"(?P<controller>\w+(?:_\w+)?)-(?P<workload_type>[\w_]+)-encryption_(?P<encryption>\w+)-logging_(?P<logging>\w+)-connection_(?P<connection>\w+)\.csv"
    data = []
    
    for filename in os.listdir(input_dir):
        if filename.endswith(".csv"):
            match = re.match(pattern, filename)
            if match:
                df = pd.read_csv(os.path.join(input_dir, filename))
                df = process_dataframe(df, match, input_dir)
                data.append(df)
    return pd.concat(data, ignore_index=True)

def process_dataframe(df, match, input_dir):
    controller = match.group("controller")
    workload_type = match.group("workload_type")
    encryption = match.group("encryption")
    logging = match.group("logging")
    connection = match.group("connection")
    
    operation_count = workload_size[workload_type.split("_")[-1]]
    df['throughput'] = operation_count / df['elapsed_time (s)']
    
    df['controller'] = controller
    df['workload_type'] = workload_type
    df['encryption'] = encryption
    df['logging'] = logging
    df['connection'] = connection
    df['operation_count'] = operation_count
    df['workload'] = df.iloc[:, 0].str.extract(r'^(workload[a-f])')[0]
    df['environment'] = 'bare_metal' if 'bare_metal' in input_dir else 'VM'
    df['variant'] = f"{controller}{'-encr' if encryption == 'ON' else '-no_encr'}{'-logging' if logging == 'ON' else '-no_logging'}{'-tcp' if connection == 'TCP' else '-unix'}"
    return df

def filter_data_for_ycsb_performance_plot(df):
    """Filter data for paper-performance plots:
    - Ignore passthrough variants
    - Consider only UNIX connection for GDPRuler variants
    """
    # Only keep variants without 'passthrough' in their names
    df_filtered = df[~df['variant'].str.contains('passthrough')].copy()
    
    # For GDPRuler variants, keep only UNIX connection
    mask_gdpruler = df_filtered['variant'].str.contains('gdpr')
    df_filtered = df_filtered[~(mask_gdpruler & (df_filtered['connection'] != 'UNIX'))]
    
    return df_filtered

def prepare_plot_data(df_subset, metric, group_by):
    values = df_subset.groupby(group_by)[f'avg_{metric} (s)' if metric == 'latency' else 'throughput'].mean()
    if metric == 'latency':
        values *= 1_000_000  # Convert to microseconds
    elif metric == 'throughput':
        values /= 1000  # Convert to kops
    return values

def sort_variants(unique_variants):
    variant_order = [
        "direct_bare_metal", "passthrough_bare_metal", "gdpr_bare_metal",
        "direct_CVM", "passthrough_CVM", "gdpr_CVM"
    ]
    # Priority mappings for nested sorting
    encryption_priority = {"no_encr": 0, "encr": 1}
    logging_priority = {"no_logging": 0, "logging": 1}
    connection_priority = {"tcp": 0, "unix": 1}

    # Extract prefix and flags from variant
    def extract_parts(variant: str):
        # Find the longest matching prefix from variant_order
        prefix = next((vo for vo in variant_order if variant.startswith(vo)), None)
        if prefix is None:
            prefix = variant.split('-')[0]
        flags = variant[len(prefix):].strip('-').split('-') if len(variant) > len(prefix) else []
        # Get flags or default values
        enc_flag = next((f for f in flags if f in encryption_priority), "no_encr")
        log_flag = next((f for f in flags if f in logging_priority), "no_logging")
        conn_flag = next((f for f in flags if f in connection_priority), "tcp")
        return (
            variant_order.index(prefix) if prefix in variant_order else float('inf'),
            encryption_priority[enc_flag],
            logging_priority[log_flag],
            connection_priority[conn_flag]
        )

    # Sort variants by all criteria
    sorted_variants = sorted(unique_variants, key=extract_parts)
    return sorted_variants

def create_ycsb_performance_plot(data, output_dir):
    """Create paper-ready plots with 2 rows (DBs) x 3 columns layout"""
    
    # Filter data according to paper requirements
    data_filtered = filter_data_for_ycsb_performance_plot(data)
    
    # Define figure size (1/3 of double column for each subplot)
    fig, axes = plt.subplots(2, 3, figsize=(figwidth_full, 3))
    
    # Only consider redis and rocksdb
    dbs = ['redis', 'rocksdb']
    
    # Get sorted variants and setup colors/hatches
    variants = sort_variants(data_filtered['variant'].unique())
    colors = sns.color_palette("pastel", n_colors=len(variants))
    
    # Data for plotting
    workloads = sorted(data_filtered['workload'].unique())
    thread_counts = sorted(data_filtered['n_clients'].unique())
    x_workloads = range(len(workloads))
    x_threads = range(len(thread_counts))
    width = 0.8 / len(variants)
    
    # Define title suffixes for each column
    title_prefixes = [
        ['(a)', '(b)', '(c)'],  # Top row (Redis)
        ['(d)', '(e)', '(f)']   # Bottom row (RocksDB)
    ]
    
    title_descriptions = [
        ['Redis - 1 Client', 'Workload A (50/50 R/W)', 'Workload C (100/0 R/W)'],  # Top row (Redis)
        ['Rocksdb - 1 Client', 'Workload A (50/50 R/W)', 'Workload C (100/0 R/W)']   # Bottom row (RocksDB)
    ]
    
    # Create plots for each DB (row)
    for i, db in enumerate(dbs):
        df_db = data_filtered[data_filtered['db'] == db]
        
        # Column 1: Throughput vs workloads (1 thread)
        ax = axes[i, 0]
        df_subset = df_db[df_db['n_clients'] == 1]
        
        for j, variant in enumerate(variants):
            df_var = df_subset[df_subset['variant'] == variant]
            if not df_var.empty:
                values = prepare_plot_data(df_var, 'throughput', 'workload')
                offset = width * j - 0.4 + width / 2
                ax.bar([xi + offset for xi in x_workloads], values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
        
        ax.set_xticks(x_workloads)
        ax.set_xticklabels([w[-1].upper() for w in workloads], fontsize=TICK_FONTSIZE)
        ax.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax.set_xlabel('YCSB Workload', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_ylabel('Throughput (kops)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_title(f"{title_prefixes[i][0]} {title_descriptions[i][0]} (Higher is better↑)", fontsize=TITLE_FONTSIZE, color="navy", pad=3)

        # Column 2: Throughput vs thread counts for workloadA (write-heavy)
        ax = axes[i, 1]
        df_subset = df_db[df_db['workload'] == 'workloada']
        
        for j, variant in enumerate(variants):
            df_var = df_subset[df_subset['variant'] == variant]
            if not df_var.empty:
                values = prepare_plot_data(df_var, 'throughput', 'n_clients')
                offset = width * j - 0.4 + width / 2
                ax.bar([xi + offset for xi in x_threads], values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
        
        ax.set_xticks(x_threads)
        ax.set_xticklabels(thread_counts, fontsize=TICK_FONTSIZE)
        ax.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax.set_xlabel('Thread Count', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_ylabel('Throughput (kops)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_title(f"{title_prefixes[i][1]} {title_descriptions[i][1]} (Higher is better↑)", fontsize=TITLE_FONTSIZE, color="navy", pad=3)

        # Column 3: Throughput vs thread counts for workloadC (read-heavy)
        ax = axes[i, 2]
        df_subset = df_db[df_db['workload'] == 'workloadc']
        
        for j, variant in enumerate(variants):
            df_var = df_subset[df_subset['variant'] == variant]
            if not df_var.empty:
                values = prepare_plot_data(df_var, 'throughput', 'n_clients')
                offset = width * j - 0.4 + width / 2
                ax.bar([xi + offset for xi in x_threads], values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
        
        ax.set_xticks(x_threads)
        ax.set_xticklabels(thread_counts, fontsize=TICK_FONTSIZE)
        ax.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax.set_xlabel('Thread Count', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_ylabel('Throughput (kops)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_title(f"{title_prefixes[i][2]} {title_descriptions[i][2]} (Higher is better↑)", fontsize=TITLE_FONTSIZE, color="navy", pad=3)

    # Add vertical database name annotations on the left of each row
    # Redis annotation for top row
    fig.text(-0.01, 0.75, 'Redis', fontsize=LABEL_FONTSIZE + 1, rotation=90, 
             verticalalignment='center', horizontalalignment='center', weight='bold')
    
    # RocksDB annotation for bottom row
    fig.text(-0.01, 0.25, 'RocksDB', fontsize=LABEL_FONTSIZE + 1, rotation=90, 
             verticalalignment='center', horizontalalignment='center', weight='bold')
    
    # Create unified legend
    handles, labels = [], []
    for j, variant in enumerate(variants):
        handles.append(plt.Rectangle((0,0),1,1, facecolor=colors[j], alpha=0.8, 
                                   hatch=hatches[j % len(hatches)],
                                   edgecolor='black'))
        # Create label with variant info
        baseline_type = variant.split('-')[0]
        label_text = variant_mapping.get(baseline_type, baseline_type)
        
        # Add encryption/logging/connection info
        if 'no_encr' in variant:
            label_text += " (w/o Encr)"
        else:
            label_text += " (w/ Encr)"
            
        labels.append(label_text)
    
    # Position legend at the top
    fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 1.11), 
              ncol=min(len(variants), 3), fontsize=LEGEND_FONTSIZE, frameon=True)
    
    # Adjust layout and save
    plt.tight_layout()
    
    os.makedirs(output_dir, exist_ok=True)
    plt.savefig(os.path.join(output_dir, 'ycsb_performance_plot.png'), 
                bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, 'ycsb_performance_plot.pdf'), 
                bbox_inches='tight')
    plt.close(fig)

def main():
    parser = argparse.ArgumentParser(description="Generate latency and throughput plots from CSV data.")
    parser.add_argument("--bare_metal_results", type=str, default="../bare_metal/results", help="Directory containing the bare metal CSV files")
    parser.add_argument("--vm_results", type=str, default="../VM/results", help="Directory containing the VM CSV files")
    parser.add_argument("--output_dir", type=str, default="plots", help="Directory to save the generated plots")
    args = parser.parse_args()

    bare_metal_data = load_data_from_directory(args.bare_metal_results)
    vm_data = load_data_from_directory(args.vm_results)
    all_data = pd.concat([bare_metal_data, vm_data], ignore_index=True)

    # ADD DEBUGGING HERE
    print("=== DATA DEBUGGING ===")
    print(f"Total data shape: {all_data.shape}")
    print(f"Columns: {all_data.columns.tolist()}")
    print(f"Unique DBs: {all_data['db'].unique()}")
    print(f"Unique workloads: {all_data['workload'].unique()}")
    print(f"Unique variants: {all_data['variant'].unique()}")
    print(f"Unique n_clients: {sorted(all_data['n_clients'].unique())}")
    print("Sample data:")
    print(all_data[['db', 'workload', 'variant', 'n_clients', 'throughput']].head(10))
    print("======================")
    
    # Create paper-performance plots
    create_ycsb_performance_plot(all_data, args.output_dir)
    
if __name__ == "__main__":
    main()
