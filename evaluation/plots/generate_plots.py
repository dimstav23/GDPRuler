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
figwidth_full = 14

FONTSIZE = 9

pastel = sns.color_palette("pastel")
# hatches = ["", "o", "*", ".", "//", "-", "\\", ".", "o-", "*-"]
hatches = ['', '///', '\\\\\\', 'xxx', '...', '+++', '', '///', '\\\\\\', 'xxx', '...', '+++']

workload_size = {
    "small": 1_000,
    "medium": 1_000_000,
    "large": 10_000_000
}

variant_mapping = {
    "direct_bare_metal"           : "Native",
    "passthrough_bare_metal"      : "Native passthrough",
    "gdpr_bare_metal"             : "Native GDPRuler",
    "direct_CVM"                  : "CVM",
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

def create_bar_plot(ax, x, values, width, offset, label, color, hatch):
    ax.bar([xi + offset for xi in x], values, width, label=label, color=color, alpha=0.8, hatch=hatch)

def set_plot_properties(ax, xlabel, ylabel, title, xticks, xticklabels):
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_xticks(xticks)
    ax.set_xticklabels(xticklabels)
    ax.legend(bbox_to_anchor=(1.05, 1), ncols=1, loc='best')

def save_plot(fig, output_dir, filename):
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, f'{filename}.png'), bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{filename}.pdf'), bbox_inches='tight')
    plt.close(fig)

def create_workload_bar_plots(data, db, metric, output_dir):
    for workload in data['workload'].unique():
        fig, ax = plt.subplots(figsize=(figwidth_full, 4.0))
        
        df_subset = data[(data['db'] == db) & (data['workload'] == workload)]
        
        variants = sort_variants(df_subset['variant'].unique())
        thread_counts = sorted(df_subset['n_clients'].unique())
        
        x = range(len(thread_counts))
        width = 0.8 / len(variants)
        
        colors = sns.color_palette("pastel", n_colors=len(variants))
        for i, variant in enumerate(variants):
            variant_data = df_subset[df_subset['variant'] == variant]
            baseline_type = variant.split('-')[0]
            if not variant_data.empty:
                values = prepare_plot_data(variant_data, metric, 'n_clients')
                offset = width * i - 0.4 + width / 2
                label = f"{variant_mapping[baseline_type]} ({'w/ Encryption' if variant_data['encryption'].iloc[0] == 'ON' else 'w/o Encryption'}"
                label = f"{label}{'-UNIX' if variant_data['connection'].iloc[0] == 'UNIX' else '-TCP'}"
                label = f"{label}{'-w/ Logging' if variant_data['logging'].iloc[0] == 'ON)' else ')'}"
                create_bar_plot(ax, x, values, width, offset, label, colors[i], hatches[i%len(hatches)])
        
        set_plot_properties(ax, 'Thread Count', 
                            f'Average {metric.capitalize()} {"(µs)" if metric == "latency" else "(kops)"}',
                            f'{db.capitalize()} - {metric.capitalize()} - Workload {workload}',
                            x, thread_counts)
        
        save_plot(fig, output_dir, f'{db}_{metric}_{workload}')

def create_thread_count_bar_plots(data, db, metric, output_dir):
    for thread_count in sorted(data['n_clients'].unique()):
        fig, ax = plt.subplots(figsize=(figwidth_full, 4.0))
        
        df_subset = data[(data['db'] == db) & (data['n_clients'] == thread_count)]
        variants = sort_variants(df_subset['variant'].unique())
        workloads = sorted(df_subset['workload'].unique())

        x = range(len(workloads))
        width = 0.8 / len(variants)
        
        colors = sns.color_palette("pastel", n_colors=len(variants))
        
        for i, variant in enumerate(variants):
            variant_data = df_subset[df_subset['variant'] == variant]
            baseline_type = variant.split('-')[0]
            if not variant_data.empty:
                values = prepare_plot_data(variant_data, metric, 'workload')
                offset = width * i - 0.4 + width / 2
                label = f"{variant_mapping[baseline_type]} ({'w/ Encryption' if variant_data['encryption'].iloc[0] == 'ON' else 'w/o Encryption'}"
                label = f"{label}{'-UNIX' if variant_data['connection'].iloc[0] == 'UNIX' else '-TCP'}"
                label = f"{label}{'-w/ Logging' if variant_data['logging'].iloc[0] == 'ON)' else ')'}"
                create_bar_plot(ax, x, values, width, offset, label, colors[i], hatches[i%len(hatches)])
        
        set_plot_properties(ax, 'Workload', 
                            f'Average {metric.capitalize()} {"(µs)" if metric == "latency" else "(kops)"}',
                            f'{db.capitalize()} - {metric.capitalize()} - {thread_count} Threads',
                            x, workloads)
        
        save_plot(fig, output_dir, f'{db}_{metric}_{thread_count}_threads')

def create_generic_bar_plots(data, output_dir):
    for db in data['db'].unique():
        db_output_dir = os.path.join(output_dir, db)
        os.makedirs(db_output_dir, exist_ok=True)
        
        for metric in ['latency', 'throughput']:
            create_workload_bar_plots(data, db, metric, db_output_dir)
            create_thread_count_bar_plots(data, db, metric, db_output_dir)

def main():
    parser = argparse.ArgumentParser(description="Generate latency and throughput plots from CSV data.")
    parser.add_argument("--bare_metal_results", type=str, default="../bare_metal/results", help="Directory containing the bare metal CSV files")
    parser.add_argument("--vm_results", type=str, default="../VM/results", help="Directory containing the VM CSV files")
    parser.add_argument("--output_dir", type=str, default="plots", help="Directory to save the generated plots")
    args = parser.parse_args()

    bare_metal_data = load_data_from_directory(args.bare_metal_results)
    vm_data = load_data_from_directory(args.vm_results)
    all_data = pd.concat([bare_metal_data, vm_data], ignore_index=True)

    # Create extensive bar plots for throughput and latency for workloads and thread counts
    create_generic_bar_plots(all_data, args.output_dir)
    
if __name__ == "__main__":
    main()
