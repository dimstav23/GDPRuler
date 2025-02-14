import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import re
import argparse

workload_size = {
    "small": 1_000,
    "medium": 1_000_000,
    "large": 10_000_000
}

variant_mapping = {
    "direct": "Direct client/server",
    "native_ctl": "Native passthrough proxy",
    "gdpr_ctl": "Native GDPRuler",
    "gdpr_self_hosted": "GDPRuler - bare-metal DB",
    "gdpr_cloud_hosted": "GDPRuler - VM DB",
    "gdpr_confidential_cloud_hosted": "GDPRuler - Confidential DB"
}

variant_order = ["direct", "native_ctl", "gdpr_ctl", "gdpr_self_hosted", "gdpr_cloud_hosted", "gdpr_confidential_cloud_hosted"]

hatches = ['', '///', '\\\\\\', 'xxx', '...', '+++', '', '///', '\\\\\\', 'xxx', '...', '+++']

def load_data_from_directory(input_dir):
    pattern = r"(?P<controller>\w+(?:_\w+)?)-(?P<workload_type>[\w_]+)-encryption_(?P<encryption>\w+)-logging_(?P<logging>\w+)\.csv"
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
    
    operation_count = workload_size[workload_type.split("_")[-1]]
    df['throughput'] = operation_count / df['elapsed_time (s)']
    
    df['controller'] = controller
    df['workload_type'] = workload_type
    df['encryption'] = encryption
    df['logging'] = logging
    df['operation_count'] = operation_count
    df['workload'] = df.iloc[:, 0].str.extract(r'^(workload[a-f])')[0]
    df['environment'] = 'bare_metal' if 'bare_metal' in input_dir else 'VM'
    df['variant'] = controller
    
    return df

def prepare_plot_data(df_subset, metric, group_by):
    values = df_subset.groupby(group_by)[f'avg_{metric} (s)' if metric == 'latency' else 'throughput'].mean()
    if metric == 'latency':
        values *= 1_000_000  # Convert to microseconds
    elif metric == 'throughput':
        values /= 1000  # Convert to kops
    return values

def create_bar_plot(ax, x, values, width, offset, label, color, hatch):
    ax.bar([xi + offset for xi in x], values, width, label=label, color=color, alpha=0.8, hatch=hatch)

def set_plot_properties(ax, xlabel, ylabel, title, xticks, xticklabels):
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.set_xticks(xticks)
    ax.set_xticklabels(xticklabels)
    ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')

def save_plot(fig, output_dir, filename):
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, f'{filename}.png'), bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{filename}.pdf'), bbox_inches='tight')
    plt.close(fig)

def create_workload_plots(data, db, metric, output_dir):
    for workload in data['workload'].unique():
        fig, ax = plt.subplots(figsize=(14, 6))
        
        df_subset = data[(data['db'] == db) & (data['workload'] == workload)]
        
        variants = [v for v in variant_order if v in df_subset['variant'].unique()]
        thread_counts = sorted(df_subset['n_clients'].unique())
        
        x = range(len(thread_counts))
        width = 0.8 / len(variants)
        
        colors = sns.color_palette("pastel", n_colors=len(variants))
        
        for i, variant in enumerate(variants):
            variant_data = df_subset[df_subset['variant'] == variant]
            if not variant_data.empty:
                values = prepare_plot_data(variant_data, metric, 'n_clients')
                offset = width * i - 0.4 + width / 2
                label = f"{variant_mapping[variant]} ({'w/ Encryption' if variant_data['encryption'].iloc[0] == 'on' else 'w/o Encryption'})"
                create_bar_plot(ax, x, values, width, offset, label, colors[i], hatches[i])
        
        set_plot_properties(ax, 'Thread Count', 
                            f'Average {metric.capitalize()} {"(µs)" if metric == "latency" else "(kops)"}',
                            f'{db.capitalize()} - {metric.capitalize()} - Workload {workload}',
                            x, thread_counts)
        
        save_plot(fig, output_dir, f'{db}_{metric}_{workload}')

def create_thread_count_plots(data, db, metric, output_dir):
    for thread_count in sorted(data['n_clients'].unique()):
        fig, ax = plt.subplots(figsize=(14, 6))
        
        df_subset = data[(data['db'] == db) & (data['n_clients'] == thread_count)]
        
        variants = [v for v in variant_order if v in df_subset['variant'].unique()]
        workloads = sorted(df_subset['workload'].unique())
        
        x = range(len(workloads))
        width = 0.8 / len(variants)
        
        colors = sns.color_palette("pastel", n_colors=len(variants))
        
        for i, variant in enumerate(variants):
            variant_data = df_subset[df_subset['variant'] == variant]
            if not variant_data.empty:
                values = prepare_plot_data(variant_data, metric, 'workload')
                offset = width * i - 0.4 + width / 2
                label = f"{variant_mapping[variant]} ({'w/ Encryption' if variant_data['encryption'].iloc[0] == 'on' else 'w/o Encryption'})"
                create_bar_plot(ax, x, values, width, offset, label, colors[i], hatches[i])
        
        set_plot_properties(ax, 'Workload', 
                            f'Average {metric.capitalize()} {"(µs)" if metric == "latency" else "(kops)"}',
                            f'{db.capitalize()} - {metric.capitalize()} - {thread_count} Threads',
                            x, workloads)
        
        save_plot(fig, output_dir, f'{db}_{metric}_{thread_count}_threads')

def create_bar_plots(data, output_dir):
    for db in data['db'].unique():
        db_output_dir = os.path.join(output_dir, db)
        os.makedirs(db_output_dir, exist_ok=True)
        
        for metric in ['latency', 'throughput']:
            create_workload_plots(data, db, metric, db_output_dir)
            create_thread_count_plots(data, db, metric, db_output_dir)

def main():
    parser = argparse.ArgumentParser(description="Generate latency and throughput plots from CSV data.")
    parser.add_argument("--bare_metal_results", type=str, default="../bare_metal/results", help="Directory containing the bare metal CSV files")
    parser.add_argument("--vm_results", type=str, default="../VM/results", help="Directory containing the VM CSV files")
    parser.add_argument("--output_dir", type=str, default="plots", help="Directory to save the generated plots")
    args = parser.parse_args()

    bare_metal_data = load_data_from_directory(args.bare_metal_results)
    vm_data = load_data_from_directory(args.vm_results)
    all_data = pd.concat([bare_metal_data, vm_data], ignore_index=True)

    create_bar_plots(all_data, args.output_dir)

if __name__ == "__main__":
    main()
