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

# Visual elements
hatches = ['', '///', '\\\\\\', 'xxx', '...', '+++', '', '///', '\\\\\\', 'xxx', '...', '+++']

def load_data_from_dirs(bare_metal_dir, cvm_dir):
    """Load GDPR queries CSV files from separate bare_metal and CVM directories.
    
    Expected filename format: {variant}-gdpr_queries-encryption_{ON/OFF}-logging_{ON/OFF}-connection_{type}.csv
    """
    # Pattern to match filenames
    pattern = r"(?P<variant>[\w_]+)-gdpr_queries-encryption_(?P<encryption>ON|OFF)-logging_(?P<logging>ON|OFF)-connection_(?P<connection>\w+)\.csv"
    
    data = []
    
    # Load bare_metal results
    if os.path.exists(bare_metal_dir):
        print(f"Loading bare_metal results from: {bare_metal_dir}")
        for filename in os.listdir(bare_metal_dir):
            if filename.endswith(".csv") and "gdpr_queries" in filename:
                match = re.match(pattern, filename)
                if match:
                    df = pd.read_csv(os.path.join(bare_metal_dir, filename))
                    
                    df['variant'] = match.group("variant")
                    df['encryption'] = match.group("encryption")
                    df['logging'] = match.group("logging")
                    df['connection'] = match.group("connection")
                    df['environment'] = 'bare_metal'
                    
                    # Calculate total operations and throughput
                    op_count_cols = [col for col in df.columns if col.endswith('_count') 
                                    and col not in ['ctl_files_count', 'db_files_count']]
                    if op_count_cols:
                        df['total_ops'] = df[op_count_cols].sum(axis=1)
                    else:
                        df['total_ops'] = 0
                    
                    df['throughput'] = df['total_ops'] / df['elapsed_time (s)']
                    df['entity'] = df['workload'].str.replace('gdpr_', '')
                    
                    data.append(df)
                    print(f"  ✓ {filename}")
    
    # Load CVM results
    if os.path.exists(cvm_dir):
        print(f"Loading CVM results from: {cvm_dir}")
        for filename in os.listdir(cvm_dir):
            if filename.endswith(".csv") and "gdpr_queries" in filename:
                match = re.match(pattern, filename)
                if match:
                    df = pd.read_csv(os.path.join(cvm_dir, filename))
                    
                    df['variant'] = match.group("variant")
                    df['encryption'] = match.group("encryption")
                    df['logging'] = match.group("logging")
                    df['connection'] = match.group("connection")
                    df['environment'] = 'CVM'
                    
                    # Calculate total operations and throughput
                    op_count_cols = [col for col in df.columns if col.endswith('_count') 
                                    and col not in ['ctl_files_count', 'db_files_count']]
                    if op_count_cols:
                        df['total_ops'] = df[op_count_cols].sum(axis=1)
                    else:
                        df['total_ops'] = 0
                    
                    df['throughput'] = df['total_ops'] / df['elapsed_time (s)']
                    df['entity'] = df['workload'].str.replace('gdpr_', '')
                    
                    data.append(df)
                    print(f"  ✓ {filename}")
    
    if data:
        combined = pd.concat(data, ignore_index=True)
        print(f"\n✓ Total records loaded: {len(combined)}")
        return combined
    else:
        print("❌ No data files found!")
        return pd.DataFrame()

def aggregate_operation_stats(df):
    """
    Aggregate per-operation statistics across multiple benchmark runs.
    
    Statistical methodology:
    When combining statistics from multiple independent runs, we compute:
    
    1. **Grand Mean (μ_combined)**: 
       Weighted average of means from all runs:
       μ_combined = Σ(n_i × μ_i) / Σn_i
       where n_i = operation count in run i, μ_i = mean latency in run i
    
    2. **Pooled Standard Deviation (σ_pooled)**:
       Combines variance from all runs:
       σ_pooled = sqrt[Σ(n_i × σ_i²) / Σn_i]
       where σ_i = standard deviation in run i
       
       This accounts for within-run variance weighted by sample size.
    """
    operation_cols = []
    for col in df.columns:
        if col.endswith('_count') and col not in ['ctl_files_count', 'db_files_count']:
            op_name = col.replace('_count', '')
            operation_cols.append(op_name)
    
    aggregated = {}
    
    for op in operation_cols:
        count_col = f"{op}_count"
        avg_col = f"{op}_avg_lat (s)"
        std_col = f"{op}_std (s)"
        
        if count_col not in df.columns or avg_col not in df.columns or std_col not in df.columns:
            continue
        
        op_data = df[df[count_col] > 0].copy()
        
        if len(op_data) == 0:
            continue
        
        total_ops = op_data[count_col].sum()
        if total_ops == 0:
            continue
            
        grand_mean = (op_data[count_col] * op_data[avg_col]).sum() / total_ops
        variance_sum = (op_data[count_col] * op_data[std_col]**2).sum()
        pooled_std = np.sqrt(variance_sum / total_ops)
        
        aggregated[op] = {
            'mean_latency_s': grand_mean,
            'pooled_std_s': pooled_std,
            'total_count': int(total_ops),
            'num_runs': len(op_data)
        }
    
    return aggregated

def create_performance_per_db_plot(data, db_name, output_dir):
    """
    Create paper-ready plot with 2 subplots:
    (a) Throughput by entity with 4 bars per entity (with error bars)
    (b) Per-operation latency with error bars (metadata operations only)
    """
    df_db = data[data['db'] == db_name].copy()
    
    if df_db.empty:
        print(f"⚠ No data for {db_name}")
        return
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(figwidth_full, 2.5))
    
    # --- Subplot (a): Throughput (now in ops/s with error bars) ---
    entities = sorted(df_db['entity'].unique())
    n_entities = len(entities)
    
    variants = [
        ('bare_metal', 'OFF', 'Native GDPRuler (w/o Encr)'),
        ('bare_metal', 'ON', 'Native GDPRuler (w/ Encr)'),
        ('CVM', 'OFF', 'CVM GDPRuler (w/o Encr)'),
        ('CVM', 'ON', 'CVM GDPRuler (w/ Encr)')
    ]
    
    x_pos = np.arange(n_entities)
    bar_width = 0.2
    colors = sns.color_palette("Set2", n_colors=4)
    
    for i, (env, encr, label) in enumerate(variants):
        throughputs = []
        throughput_stds = []
        elapsed_times = []
        
        for entity in entities:
            subset = df_db[(df_db['entity'] == entity) & 
                          (df_db['environment'] == env) & 
                          (df_db['encryption'] == encr)]
            
            if not subset.empty:
                throughputs.append(subset['throughput'].mean())
                throughput_stds.append(subset['throughput'].std())
                elapsed_times.append(subset['elapsed_time (s)'].mean())
            else:
                throughputs.append(0)
                throughput_stds.append(0)
                elapsed_times.append(0)
        
        bar_positions = x_pos + i * bar_width - 0.3
        bars = ax1.bar(bar_positions, throughputs, bar_width,
                       label=label, color=colors[i], 
                       hatch=hatches[i % len(hatches)],
                       alpha=0.7, edgecolor='black', linewidth=0.5)
        
        # Add error bars
        ax1.errorbar(bar_positions, throughputs, yerr=throughput_stds,
                    fmt='none', ecolor='black', capsize=2, lw=0.8, capthick=0.8)
        
        # Add completion time annotations
        for j, (bar, etime) in enumerate(zip(bars, elapsed_times)):
            if etime > 0:
                height = bar.get_height()
                ax1.text(bar.get_x() + bar.get_width()/2., height,
                        f'{etime:.0f}s',
                        ha='center', va='bottom', 
                        fontsize=ANNOTATION_FONTSIZE, rotation=0)
    
    ax1.set_xlabel('GDPR Workload', fontsize=LABEL_FONTSIZE, labelpad=2)
    ax1.set_ylabel('Throughput (ops/s)', fontsize=LABEL_FONTSIZE, labelpad=2)
    ax1.set_title(f'(a) {db_name.capitalize()} Throughput', 
                  fontsize=TITLE_FONTSIZE, pad=3)
    ax1.set_xticks(x_pos + bar_width * 1.5)
    ax1.set_xticklabels([e.capitalize() for e in entities], fontsize=TICK_FONTSIZE)
    ax1.tick_params(axis='x', length=0, pad=2)
    ax1.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
    ax1.grid(True, alpha=0.3, axis='y')
    ax1.legend(fontsize=LEGEND_FONTSIZE, loc='upper left', framealpha=0.9, ncol=1)
    
    # --- Subplot (b): Per-operation latency (metadata operations only) ---
    ops_agg = aggregate_operation_stats(df_db)
    
    # Filter only operations ending with 'M' (metadata operations)
    metadata_ops = {k: v for k, v in ops_agg.items() if k.upper().endswith('M')}
    
    if metadata_ops:
        op_names = []
        op_means = []
        op_stds = []
        
        for op_name in sorted(metadata_ops.keys()):
            stats_dict = metadata_ops[op_name]
            op_names.append(op_name.upper())
            op_means.append(stats_dict['mean_latency_s'] * 1000)  # to ms
            op_stds.append(stats_dict['pooled_std_s'] * 1000)
        
        x_ops = np.arange(len(op_names))
        bars = ax2.bar(x_ops, op_means, color='steelblue', alpha=0.7, 
                edgecolor='black', linewidth=0.5)
        ax2.errorbar(x_ops, op_means, yerr=op_stds,
                    fmt='none', ecolor='black', capsize=3, lw=1, capthick=1)
        
        ax2.set_xlabel('Metadata Operation', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax2.set_ylabel('Avg Latency (ms)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax2.set_title(f'(b) {db_name.capitalize()} Operation Latency', 
                      fontsize=TITLE_FONTSIZE, pad=3)
        ax2.set_xticks(x_ops)
        ax2.set_xticklabels(op_names, fontsize=TICK_FONTSIZE, rotation=45, ha='right')
        ax2.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax2.grid(True, alpha=0.3, axis='y')
    else:
        ax2.text(0.5, 0.5, 'No metadata operations found', 
                ha='center', va='center', transform=ax2.transAxes,
                fontsize=LABEL_FONTSIZE)
        ax2.set_xlabel('Metadata Operation', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax2.set_ylabel('Avg Latency (ms)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax2.set_title(f'(b) {db_name.capitalize()} Operation Latency', 
                      fontsize=TITLE_FONTSIZE, pad=3)
    
    plt.tight_layout()
    
    output_filename = f'gdpr_{db_name}_performance'
    plt.savefig(os.path.join(output_dir, f'{output_filename}.png'), dpi=300, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_filename}.pdf'), bbox_inches='tight')
    plt.close()
    
    print(f"  ✓ Paper-ready plot: {output_filename}")

def create_gdpr_queries_performance_plot(data, output_dir):
    """
    Create combined paper-ready plot with both Redis and RocksDB.
    Layout: 2 rows (Redis top, RocksDB bottom)
    Each row: 75% throughput (left), 25% latency (right)
    """
    databases = sorted(data['db'].unique())
    
    if len(databases) < 2:
        print("⚠ Need both redis and rocksdb data for combined plot")
        return
    
    # Create figure with custom width ratios: 3:1
    fig = plt.figure(figsize=(figwidth_half, 2.5))
    gs = fig.add_gridspec(2, 2, width_ratios=[4, 1], height_ratios=[1, 1], 
                          hspace=0.6, wspace=0.5)
    
    # Create axes
    ax_redis_throughput = fig.add_subplot(gs[0, 0])
    ax_redis_latency = fig.add_subplot(gs[0, 1])
    ax_rocksdb_throughput = fig.add_subplot(gs[1, 0])
    ax_rocksdb_latency = fig.add_subplot(gs[1, 1])
    
    axes_pairs = [
        ('redis', ax_redis_throughput, ax_redis_latency, '(a)', '(b)'),
        ('rocksdb', ax_rocksdb_throughput, ax_rocksdb_latency, '(c)', '(d)')
    ]
    
    for db_name, ax_throughput, ax_latency, label_left, label_right in axes_pairs:
        df_db = data[data['db'] == db_name].copy()
        
        if df_db.empty:
            continue
        
        # --- Throughput subplot ---
        entities = sorted(df_db['entity'].unique())
        n_entities = len(entities)
        
        variants = [
            ('bare_metal', 'OFF', 'Native GDPRuler (w/o Encr)'),
            ('bare_metal', 'ON', 'Native GDPRuler (w/ Encr)'),
            ('CVM', 'OFF', 'CVM GDPRuler (w/o Encr)'),
            ('CVM', 'ON', 'CVM GDPRuler (w/ Encr)')
        ]
        # Get sorted variants and setup colors/hatches
        colors = sns.color_palette("pastel", n_colors=len(variants))
        x_pos = np.arange(n_entities)
        bar_width = 0.8 / len(variants)
        
        # Collect all max values
        all_max_values = []
        for i, (env, encr, label) in enumerate(variants):
            throughputs = []
            throughput_stds = []
            elapsed_times = []
            
            for entity in entities:
                subset = df_db[(df_db['entity'] == entity) & 
                              (df_db['environment'] == env) & 
                              (df_db['encryption'] == encr)]
                
                if not subset.empty:
                    throughput = subset['throughput'].mean()
                    std = subset['throughput'].std()
                    all_max_values.append(throughput + std)
                    throughputs.append(throughput)
                    throughput_stds.append(std)
                    elapsed_times.append(subset['elapsed_time (s)'].mean())
                else:
                    throughputs.append(0)
                    throughput_stds.append(0)
                    elapsed_times.append(0)
                    
            bar_positions = x_pos + i * bar_width - 0.3
            bars = ax_throughput.bar(bar_positions, throughputs, bar_width,
                           label=label, color=colors[i], 
                           hatch=hatches[i % len(hatches)],
                           alpha=0.8, edgecolor='black')
            
            ax_throughput.errorbar(bar_positions, throughputs, yerr=throughput_stds,
                        fmt='none', ecolor='black', capsize=1, lw=0.5)
            
            # Add completion time annotations (moved after error bars to position correctly)
            for j, (bar, etime, std) in enumerate(zip(bars, elapsed_times, throughput_stds)):
                if etime > 0:
                    height = bar.get_height()
                    # Position annotation above the error bar (height + std)
                    y_position = height + std + (max(throughputs) * 0.02)  # Add small offset
                    ax_throughput.text(bar.get_x() + bar.get_width()/2., y_position,
                            f'{etime:.0f}s',
                            ha='center', va='bottom', 
                            fontsize=ANNOTATION_FONTSIZE + 1, rotation=0)
        
        ax_throughput.set_xlabel('GDPR Workload', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax_throughput.set_ylabel('Throughput (ops/s)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax_throughput.set_title(f'{label_left} Throughput (Higher is better↑)', color="navy",
                      fontsize=TITLE_FONTSIZE, pad=3)
        ax_throughput.set_xticks(x_pos)
        ax_throughput.set_xticklabels([e.capitalize() for e in entities], fontsize=TICK_FONTSIZE)
        ax_throughput.tick_params(axis='x', length=0, pad=2)
        ax_throughput.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax_throughput.grid(True, alpha=0.3, axis='y')

        # Only show legend on top plot
        if db_name == 'redis':
            ax_throughput.legend(loc='upper center', bbox_to_anchor=(0.8, 1.7), 
              ncol=len(variants)/2, fontsize=LEGEND_FONTSIZE, frameon=True)
        
        if all_max_values:
          max_y = max(all_max_values)
          ax_throughput.set_ylim(0, max_y * 1.1)  # 15% headroom
          
        # --- Latency subplot (metadata operations only) ---
        ops_agg = aggregate_operation_stats(df_db)
        metadata_ops = {k: v for k, v in ops_agg.items() if k.upper().endswith('M')}
        
        if metadata_ops:
            op_names = []
            op_means = []
            op_stds = []
            
            for op_name in metadata_ops.keys():
                stats_dict = metadata_ops[op_name]
                op_names.append(op_name.upper())
                op_means.append(stats_dict['mean_latency_s'] * 1000)
                op_stds.append(stats_dict['pooled_std_s'] * 1000)
            
            x_ops = np.arange(len(op_names))
            bars = ax_latency.bar(x_ops, op_means, color='steelblue', alpha=0.8, 
                    edgecolor='black')
            ax_latency.errorbar(x_ops, op_means, yerr=op_stds,
                        fmt='none', ecolor='black', capsize=1, lw=0.5)
            
            # ax_latency.set_xlabel('Metadata Op.', fontsize=LABEL_FONTSIZE, labelpad=2)
            ax_latency.set_xlabel("")
            ax_latency.set_ylabel('Avg Latency (ms)', fontsize=LABEL_FONTSIZE, labelpad=2)
            ax_latency.set_title(f'{label_right} Operation Latency\n(Lower is better↓)',  color="navy",
                          fontsize=TITLE_FONTSIZE, pad=3)
            ax_latency.set_xticks(x_ops)
            ax_latency.set_xticklabels(op_names, fontsize=TICK_FONTSIZE, rotation=30)
            ax_latency.tick_params(axis='x', length=0, pad=2)
            ax_latency.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
            ax_latency.grid(True, alpha=0.3, axis='y')
        else:
            ax_latency.text(0.5, 0.5, 'No metadata\noperations', 
                    ha='center', va='center', transform=ax_latency.transAxes,
                    fontsize=TICK_FONTSIZE)
            # ax_latency.set_xlabel('Metadata Op.', fontsize=LABEL_FONTSIZE, labelpad=2)
            ax_latency.set_xlabel("")
            ax_latency.set_ylabel('Avg Latency (ms)', fontsize=LABEL_FONTSIZE, labelpad=2)
            ax_latency.set_title(f'{label_right} Operation Latency', 
                          fontsize=TITLE_FONTSIZE, pad=3)
    
    # Redis annotation for top row
    fig.text(0.01, 0.75, 'Redis', fontsize=LABEL_FONTSIZE + 1, rotation=90, 
             verticalalignment='center', horizontalalignment='center', weight='bold')
    
    # RocksDB annotation for bottom row
    fig.text(0.01, 0.25, 'RocksDB', fontsize=LABEL_FONTSIZE + 1, rotation=90, 
             verticalalignment='center', horizontalalignment='center', weight='bold')
    # Save combined plot
    output_filename = 'gdpr_combined_performance'
    plt.savefig(os.path.join(output_dir, f'{output_filename}.png'), dpi=300, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_filename}.pdf'), bbox_inches='tight')
    plt.close()
    
    print(f"  ✓ Combined paper-ready plot: {output_filename}")


def create_analytical_plots(data, output_dir):
    """Create descriptive plots for analysis."""
    
    for db_name in data['db'].unique():
        df_db = data[data['db'] == db_name]
        
        # Plot 1: Throughput heatmap
        fig, ax = plt.subplots(figsize=(5, 3))
        
        pivot_data = df_db.pivot_table(
            values='throughput',
            index='entity',
            columns=['environment', 'encryption'],
            aggfunc='mean'
        )
        
        sns.heatmap(pivot_data, annot=True, fmt='.2f', cmap='YlGnBu', ax=ax,
                   cbar_kws={'label': 'Throughput (ops/s)'}, 
                   annot_kws={'fontsize': FONTSIZE})
        ax.set_title(f'{db_name.capitalize()} - Throughput Heatmap', fontsize=TITLE_FONTSIZE)
        ax.set_xlabel('Configuration', fontsize=LABEL_FONTSIZE)
        ax.set_ylabel('Entity', fontsize=LABEL_FONTSIZE)
        ax.tick_params(labelsize=TICK_FONTSIZE)
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f'{db_name}_throughput_heatmap.png'), dpi=300, bbox_inches='tight')
        plt.savefig(os.path.join(output_dir, f'{db_name}_throughput_heatmap.pdf'), bbox_inches='tight')
        plt.close()
        
        # Plot 2: Completion time comparison
        fig, ax = plt.subplots(figsize=(figwidth_half, 3))
        
        entities = sorted(df_db['entity'].unique())
        configs = df_db.groupby(['environment', 'encryption']).size().index
        
        x = np.arange(len(entities))
        width = 0.2
        
        for i, (env, encr) in enumerate(configs):
            times = []
            for entity in entities:
                subset = df_db[(df_db['entity'] == entity) & 
                              (df_db['environment'] == env) & 
                              (df_db['encryption'] == encr)]
                times.append(subset['elapsed_time (s)'].mean() if not subset.empty else 0)
            
            label = f"{env.replace('bare_metal', 'Native')} {'Encr' if encr == 'ON' else 'No Encr'}"
            ax.bar(x + i * width - 0.3, times, width, label=label, alpha=0.7, edgecolor='black', linewidth=0.5)
        
        ax.set_xlabel('Entity', fontsize=LABEL_FONTSIZE)
        ax.set_ylabel('Elapsed Time (s)', fontsize=LABEL_FONTSIZE)
        ax.set_title(f'{db_name.capitalize()} - Completion Time', fontsize=TITLE_FONTSIZE)
        ax.set_xticks(x)
        ax.set_xticklabels([e.capitalize() for e in entities], fontsize=TICK_FONTSIZE)
        ax.tick_params(labelsize=TICK_FONTSIZE)
        ax.legend(fontsize=LEGEND_FONTSIZE, loc='best')
        ax.grid(True, alpha=0.3, axis='y')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f'{db_name}_elapsed_time.png'), dpi=300, bbox_inches='tight')
        plt.savefig(os.path.join(output_dir, f'{db_name}_elapsed_time.pdf'), bbox_inches='tight')
        plt.close()
        
        # Plot 3: Per-operation latency by entity
        entities_list = sorted(df_db['entity'].unique())
        n_entities = len(entities_list)
        
        fig, axes = plt.subplots(1, n_entities, 
                                figsize=(figwidth_full, 2.5), sharey=True)
        
        if n_entities == 1:
            axes = [axes]
        
        for idx, entity in enumerate(entities_list):
            ax = axes[idx]
            entity_data = df_db[df_db['entity'] == entity]
            
            ops_agg = aggregate_operation_stats(entity_data)
            
            if ops_agg:
                op_names = sorted(ops_agg.keys())
                means = [ops_agg[op]['mean_latency_s'] * 1000 for op in op_names]
                stds = [ops_agg[op]['pooled_std_s'] * 1000 for op in op_names]
                
                x_ops = np.arange(len(op_names))
                ax.bar(x_ops, means, color='lightcoral', alpha=0.7, edgecolor='black', linewidth=0.5)
                ax.errorbar(x_ops, means, yerr=stds, fmt='none', ecolor='black', capsize=2, lw=0.8)
                
                ax.set_xlabel('Operation', fontsize=LABEL_FONTSIZE)
                if idx == 0:
                    ax.set_ylabel('Latency (ms)', fontsize=LABEL_FONTSIZE)
                ax.set_title(f'{entity.capitalize()}', fontsize=TITLE_FONTSIZE)
                ax.set_xticks(x_ops)
                ax.set_xticklabels([op.upper() for op in op_names], 
                                  rotation=45, ha='right', fontsize=TICK_FONTSIZE)
                ax.tick_params(axis='y', labelsize=TICK_FONTSIZE)
                ax.grid(True, alpha=0.3, axis='y')
        
        fig.suptitle(f'{db_name.capitalize()} - Per-Operation Latency by Entity', 
                    fontsize=FONTSIZE + 1, y=1.02)
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f'{db_name}_ops_by_entity.png'), dpi=300, bbox_inches='tight')
        plt.savefig(os.path.join(output_dir, f'{db_name}_ops_by_entity.pdf'), bbox_inches='tight')
        plt.close()
        
        print(f"  ✓ Descriptive plots for {db_name}")

def print_statistics(data):
    """Print comprehensive statistics."""
    
    print("\n" + "="*80)
    print("GDPR QUERIES PERFORMANCE STATISTICS")
    print("="*80)
    
    for db_name in sorted(data['db'].unique()):
        df_db = data[data['db'] == db_name]
        
        print(f"\n{db_name.upper()} DATABASE:")
        print("-" * 40)
        
        for entity in sorted(df_db['entity'].unique()):
            print(f"\n  Entity: {entity.upper()}")
            entity_data = df_db[df_db['entity'] == entity]
            
            for (env, encr), group in entity_data.groupby(['environment', 'encryption']):
                config = f"{env.replace('bare_metal', 'Native')} {'w/ Encr' if encr == 'ON' else 'w/o Encr'}"
                print(f"\n    {config}:")
                print(f"      Throughput:   {group['throughput'].mean():.2f} ± {group['throughput'].std():.2f} ops/s")
                print(f"      Elapsed time: {group['elapsed_time (s)'].mean():.2f} ± {group['elapsed_time (s)'].std():.2f} s")
                print(f"      Avg latency:  {group['avg_latency (s)'].mean()*1000:.2f} ± {group['avg_latency (s)'].std()*1000:.2f} ms")
                print(f"      Runs: {len(group)}")
            
            ops_agg = aggregate_operation_stats(entity_data)
            if ops_agg:
                print(f"\n    Per-Operation Statistics:")
                for op_name, stats in sorted(ops_agg.items()):
                    print(f"      {op_name.upper()}:")
                    print(f"        Total ops: {stats['total_count']} ({stats['num_runs']} runs)")
                    print(f"        Avg latency: {stats['mean_latency_s']*1000:.3f} ms")
                    print(f"        Pooled std:  {stats['pooled_std_s']*1000:.3f} ms")

def main():
    parser = argparse.ArgumentParser(description="Generate GDPR queries performance plots")
    parser.add_argument("--bare_metal_results", type=str, default="../bare_metal/results", help="Directory containing the bare metal CSV files")
    parser.add_argument("--vm_results", type=str, default="../VM/results", help="Directory containing the VM CSV files")
    parser.add_argument("--output_dir", type=str, default="plots", help="Directory to save the generated plots")
    args = parser.parse_args()
    
    os.makedirs(args.output_dir, exist_ok=True)
    
    print(f"\n{'='*80}")
    print("GDPR QUERIES PERFORMANCE ANALYSIS")
    print(f"{'='*80}\n")
    
    data = load_data_from_dirs(args.bare_metal_results, args.vm_results)
    
    if data.empty:
        print("No data found!")
        return
    
    print(f"\n{'='*80}")
    print("DATA SUMMARY")
    print(f"{'='*80}")
    print(f"Total records: {len(data)}")
    print(f"Databases: {sorted(data['db'].unique())}")
    print(f"Entities: {sorted(data['entity'].unique())}")
    print(f"Environments: {sorted(data['environment'].unique())}")
    print(f"Encryption: {sorted(data['encryption'].unique())}")
    
    print_statistics(data)
    
    print(f"\n{'='*80}")
    print("GENERATING PLOTS")
    print(f"{'='*80}\n")
    
    # Create individual plots for each database
    for db in sorted(data['db'].unique()):
        create_performance_per_db_plot(data, db, args.output_dir)
    
    # Create combined plot with both databases
    create_gdpr_queries_performance_plot(data, args.output_dir)
    
    create_analytical_plots(data, args.output_dir)
    
    print(f"\n{'='*80}")
    print(f"✓ All plots saved to: {args.output_dir}")
    print(f"{'='*80}\n")

if __name__ == "__main__":
    main()
