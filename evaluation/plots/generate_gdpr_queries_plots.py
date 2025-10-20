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
ANNOTATION_FONTSIZE = FONTSIZE / 2

# Visual elements
hatches = ['', '///', '\\\\\\', 'xxx', '...', '+++', '', '///', '\\\\\\', 'xxx', '...', '+++']

def load_data_from_dirs(bare_metal_dir, cvm_dir):
    """Load GDPR queries CSV files from separate bare_metal and CVM directories."""
    # Updated pattern to handle both regular and metadata_indexes variants
    pattern = r"(?P<variant>[\w_]+)-gdpr_queries(?P<indexes>_metadata_indexes)?-encryption_(?P<encryption>ON|OFF)-logging_(?P<logging>ON|OFF)-connection_(?P<connection>\w+)\.csv"
    
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
                    # New field to track metadata indexes
                    df['metadata_indexes'] = True if match.group("indexes") else False
                    
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
                    indexes_label = " (w/ metadata indexes)" if df['metadata_indexes'].iloc[0] else ""
                    print(f"  ✓ {filename}{indexes_label}")
    
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
                    df['metadata_indexes'] = True if match.group("indexes") else False
                    
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
                    indexes_label = " (w/ metadata indexes)" if df['metadata_indexes'].iloc[0] else ""
                    print(f"  ✓ {filename}{indexes_label}")
    
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
    (b) Per-Query Latency with error bars (metadata operations only)
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
        ('CVM', 'OFF', 'GDPRuler (w/o Encr)'),
        ('CVM', 'ON', 'GDPRuler (w/ Encr)')
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
    
    # --- Subplot (b): Per-Query Latency (metadata operations only) ---
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
        ax2.set_title(f'(b) {db_name.capitalize()} Query Latency', 
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
        ax2.set_title(f'(b) {db_name.capitalize()} Query Latency', 
                      fontsize=TITLE_FONTSIZE, pad=3)
    
    plt.tight_layout()
    
    output_filename = f'gdpr_{db_name}_performance'
    plt.savefig(os.path.join(output_dir, f'{output_filename}.png'), dpi=300, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_filename}.pdf'), bbox_inches='tight')
    plt.close()
    
    print(f"  ✓ Paper-ready plot: {output_filename}")

def print_gdpr_workload_statistics(data):
    """
    Comprehensive GDPR workload statistics - Native vs CVM
    """
    
    print("\n" + "="*100)
    print(" "*30 + "GDPR WORKLOAD PERFORMANCE STATISTICS")
    print("="*100)
    
    data_filtered = data[data['encryption'] == 'ON'].copy()
    
    dbs = sorted(data_filtered['db'].unique())
    entities = sorted(data_filtered['entity'].unique())
    environments = sorted(data_filtered['environment'].unique())
    
    print(f"\nDatabases: {dbs}")
    print(f"Workloads: {entities}")
    print(f"Environments: {environments}\n")
    
    # ========== 1. THROUGHPUT BY ENVIRONMENT ==========
    print("="*100)
    print("1. THROUGHPUT ANALYSIS (ops/sec)")
    print("="*100)
    
    for env in environments:
        print(f"\n{'='*50}")
        print(f"ENVIRONMENT: {env.upper()}")
        print(f"{'='*50}")
        
        for db in dbs:
            print(f"\n{db.upper()}:")
            print(f"{'Workload':<15} {'No Index':>12} {'With Index':>12} {'Improvement':>12} {'Speedup':>10}")
            print("-" * 65)
            
            df_env_db = data_filtered[(data_filtered['environment'] == env) & 
                                      (data_filtered['db'] == db)]
            
            for entity in entities:
                df_entity = df_env_db[df_env_db['entity'] == entity]
                
                no_idx = df_entity[df_entity['metadata_indexes'] == False]['throughput'].values
                with_idx = df_entity[df_entity['metadata_indexes'] == True]['throughput'].values
                
                if len(no_idx) > 0 and len(with_idx) > 0:
                    improvement = ((with_idx.mean() - no_idx.mean()) / no_idx.mean() * 100)
                    speedup = with_idx.mean() / no_idx.mean()
                    
                    print(f"{entity.capitalize():<15} {no_idx.mean():>10.2f} ops {with_idx.mean():>10.2f} ops "
                          f"{improvement:>10.1f}% {speedup:>9.2f}x")
    
    # ========== 2. OPERATION LATENCIES ==========
    print("\n" + "="*100)
    print("2. OPERATION LATENCY BREAKDOWN (milliseconds)")
    print("="*100)
    
    # Define operations to analyze
    operations = ['put', 'get', 'delete', 'putm', 'getm', 'deletem']
    
    for env in environments:
        print(f"\n{'='*50}")
        print(f"ENVIRONMENT: {env.upper()}")
        print(f"{'='*50}")
        
        for db in dbs:
            print(f"\n{db.upper()}:")
            print(f"{'Operation':<15} {'No Index (ms)':>15} {'With Index (ms)':>17} {'Reduction %':>12} {'Speedup':>10}")
            print("-" * 75)
            
            df_env_db = data_filtered[(data_filtered['environment'] == env) & 
                                      (data_filtered['db'] == db)]
            
            for op in operations:
                count_col = f"{op}_count"
                # NOTE: Column name has space before (s)
                lat_col = f"{op}_avg_lat (s)"
                
                if count_col in df_env_db.columns and lat_col in df_env_db.columns:
                    # No index
                    df_no_idx = df_env_db[df_env_db['metadata_indexes'] == False]
                    total_count_no = df_no_idx[count_col].sum()
                    
                    if total_count_no > 0:
                        weighted_lat_no_s = (df_no_idx[count_col] * df_no_idx[lat_col]).sum() / total_count_no
                        lat_no_ms = weighted_lat_no_s * 1000
                    else:
                        lat_no_ms = None
                    
                    # With index
                    df_with_idx = df_env_db[df_env_db['metadata_indexes'] == True]
                    total_count_with = df_with_idx[count_col].sum()
                    
                    if total_count_with > 0:
                        weighted_lat_with_s = (df_with_idx[count_col] * df_with_idx[lat_col]).sum() / total_count_with
                        lat_with_ms = weighted_lat_with_s * 1000
                    else:
                        lat_with_ms = None
                    
                    if lat_no_ms is not None and lat_with_ms is not None and lat_no_ms > 0:
                        reduction = ((lat_no_ms - lat_with_ms) / lat_no_ms * 100)
                        speedup_lat = lat_no_ms / lat_with_ms
                        print(f"{op.upper():<15} {lat_no_ms:>15.2f} {lat_with_ms:>17.2f} "
                              f"{reduction:>10.1f}% {speedup_lat:>9.2f}x")
    
    # ========== 3. NATIVE vs CVM COMPARISON ==========
    print("\n" + "="*100)
    print("3. NATIVE vs CVM COMPARISON (with metadata indexes enabled)")
    print("="*100)
    
    for db in dbs:
        print(f"\n{db.upper()} - Throughput:")
        print(f"{'Workload':<15} {'Native':>12} {'CVM':>12} {'CVM Overhead':>15}")
        print("-" * 60)
        
        for entity in entities:
            df_native = data_filtered[(data_filtered['db'] == db) & 
                                      (data_filtered['entity'] == entity) &
                                      (data_filtered['environment'] == 'bare_metal') &
                                      (data_filtered['metadata_indexes'] == True)]
            
            df_cvm = data_filtered[(data_filtered['db'] == db) & 
                                   (data_filtered['entity'] == entity) &
                                   (data_filtered['environment'] == 'CVM') &
                                   (data_filtered['metadata_indexes'] == True)]
            
            if len(df_native) > 0 and len(df_cvm) > 0:
                native_mean = df_native['throughput'].mean()
                cvm_mean = df_cvm['throughput'].mean()
                overhead = ((native_mean - cvm_mean) / native_mean * 100)
                
                print(f"{entity.capitalize():<15} {native_mean:>10.2f} ops {cvm_mean:>10.2f} ops {overhead:>13.1f}%")
    
    # ========== 4. SUMMARY STATISTICS ==========
    print("\n" + "="*100)
    print("4. SUMMARY STATISTICS")
    print("="*100)
    
    # Indexing improvements by environment
    print("\nThroughput Improvements from Metadata Indexing:")
    print("-" * 70)
    
    for env in environments:
        improvements = []
        speedups = []
        
        for db in dbs:
            for entity in entities:
                df_entity = data_filtered[(data_filtered['environment'] == env) & 
                                          (data_filtered['db'] == db) &
                                          (data_filtered['entity'] == entity)]
                
                no_idx = df_entity[df_entity['metadata_indexes'] == False]['throughput'].values
                with_idx = df_entity[df_entity['metadata_indexes'] == True]['throughput'].values
                
                if len(no_idx) > 0 and len(with_idx) > 0:
                    improvement = ((with_idx.mean() - no_idx.mean()) / no_idx.mean() * 100)
                    speedup = with_idx.mean() / no_idx.mean()
                    improvements.append(improvement)
                    speedups.append(speedup)
        
        if improvements:
            print(f"\n{env.upper()}:")
            print(f"  Improvement: {min(improvements):.1f}% to {max(improvements):.1f}% "
                  f"(avg: {sum(improvements)/len(improvements):.1f}%)")
            print(f"  Speedup: {min(speedups):.2f}x to {max(speedups):.2f}x "
                  f"(avg: {sum(speedups)/len(speedups):.2f}x)")
    
    # CVM overhead
    print("\nCVM Overhead vs Native (with metadata indexes enabled):")
    print("-" * 70)
    
    all_overheads = []
    for db in dbs:
        for entity in entities:
            df_native = data_filtered[(data_filtered['db'] == db) & 
                                      (data_filtered['entity'] == entity) &
                                      (data_filtered['environment'] == 'bare_metal') &
                                      (data_filtered['metadata_indexes'] == True)]
            
            df_cvm = data_filtered[(data_filtered['db'] == db) & 
                                   (data_filtered['entity'] == entity) &
                                   (data_filtered['environment'] == 'CVM') &
                                   (data_filtered['metadata_indexes'] == True)]
            
            if len(df_native) > 0 and len(df_cvm) > 0:
                overhead = ((df_native['throughput'].mean() - df_cvm['throughput'].mean()) / 
                           df_native['throughput'].mean() * 100)
                all_overheads.append(overhead)
    
    if all_overheads:
        print(f"  Range: {min(all_overheads):.1f}% to {max(all_overheads):.1f}%")
        print(f"  Average: {sum(all_overheads)/len(all_overheads):.1f}%")
    
    # Latency reductions for GDPR operations
    print("\nLatency Reductions for GDPR Operations (getm, putm, deletem):")
    print("-" * 70)
    
    gdpr_ops = ['getm', 'putm', 'deletem']
    
    for env in environments:
        all_reductions = []
        
        for db in dbs:
            df_env_db = data_filtered[(data_filtered['environment'] == env) & 
                                      (data_filtered['db'] == db)]
            
            for op in gdpr_ops:
                count_col = f"{op}_count"
                lat_col = f"{op}_avg_lat (s)"
                
                if count_col in df_env_db.columns and lat_col in df_env_db.columns:
                    df_no = df_env_db[df_env_db['metadata_indexes'] == False]
                    df_yes = df_env_db[df_env_db['metadata_indexes'] == True]
                    
                    count_no = df_no[count_col].sum()
                    count_yes = df_yes[count_col].sum()
                    
                    if count_no > 0 and count_yes > 0:
                        lat_no = (df_no[count_col] * df_no[lat_col]).sum() / count_no * 1000
                        lat_yes = (df_yes[count_col] * df_yes[lat_col]).sum() / count_yes * 1000
                        
                        if lat_no > 0:
                            reduction = ((lat_no - lat_yes) / lat_no * 100)
                            all_reductions.append(reduction)
        
        if all_reductions:
            print(f"\n{env.upper()}:")
            print(f"  Reduction: {min(all_reductions):.1f}% to {max(all_reductions):.1f}% "
                  f"(avg: {sum(all_reductions)/len(all_reductions):.1f}%)")
    
    print("\n" + "="*100 + "\n")

def create_gdpr_queries_metadata_index_throughput_only_plot(data, output_dir, db_name):
    """
    Create throughput-only plot for a single database (Redis or RocksDB).
    Layout: Single plot showing throughput comparison
    Font sizes increased by 1.
    """
    # Filter to only use encryption=ON data for consistency and specified database
    data_filtered = data[(data['encryption'] == 'ON') & (data['db'] == db_name)].copy()

    if data_filtered.empty:
        print(f"⚠ No data for {db_name}")
        return

    # Increase all font sizes by 1
    FONTSIZE_SEP = FONTSIZE + 1
    TITLE_FONTSIZE_SEP = TITLE_FONTSIZE + 1
    LABEL_FONTSIZE_SEP = LABEL_FONTSIZE + 1
    TICK_FONTSIZE_SEP = TICK_FONTSIZE + 1
    LEGEND_FONTSIZE_SEP = LEGEND_FONTSIZE + 1
    ANNOTATION_FONTSIZE_SEP = ANNOTATION_FONTSIZE + 1

    # Create single plot figure
    fig, ax_throughput = plt.subplots(1, 1, figsize=(figwidth_half, 1.2))

    # --- Throughput subplot ---
    entities = sorted(data_filtered['entity'].unique())
    n_entities = len(entities)

    variants = [
        ('bare_metal', False, 'Native GDPRuler'),
        ('bare_metal', True, 'Native GDPRuler (w/ indexes)'),
        ('CVM', False, 'GDPRuler'),
        ('CVM', True, 'GDPRuler (w/ indexes)')
    ]

    sample_colors = sns.color_palette("pastel", n_colors=3*len(variants))
    colors = [sample_colors[2], sample_colors[6], sample_colors[5], sample_colors[8]]
    x_pos = np.arange(n_entities)
    bar_width = 0.8 / len(variants)

    variant_data = {}
    all_max_values = []
    for i, (env, has_indexes, label) in enumerate(variants):
        throughputs = []
        throughput_stds = []

        for entity in entities:
            subset = data_filtered[(data_filtered['entity'] == entity) &
                          (data_filtered['environment'] == env) &
                          (data_filtered['metadata_indexes'] == has_indexes)]

            if not subset.empty:
                throughput = subset['throughput'].mean()
                std = subset['throughput'].std()
                all_max_values.append(throughput + std)
                throughputs.append(throughput)
                throughput_stds.append(std)

                if entity not in variant_data:
                    variant_data[entity] = {}
                variant_data[entity][(env, has_indexes)] = throughput
            else:
                throughputs.append(0)
                throughput_stds.append(0)

        bar_positions = x_pos + i * bar_width - 0.3
        bars = ax_throughput.bar(bar_positions, throughputs, bar_width,
                      label=label, color=colors[i],
                      hatch=hatches[i % len(hatches)],
                      alpha=0.8, edgecolor='black')

        ax_throughput.errorbar(bar_positions, throughputs, yerr=throughput_stds,
                    fmt='none', ecolor='black', capsize=1, lw=0.5)

        for j, (bar, throughput) in enumerate(zip(bars, throughputs)):
            if throughput > 0:
                height = bar.get_height()
                ax_throughput.text(bar.get_x() + bar.get_width()/2., height * 1.1,
                        f'{throughput:.0f}',
                        ha='center', va='bottom',
                        fontsize=ANNOTATION_FONTSIZE_SEP, rotation=0)

    ax_throughput.set_yscale('log')
    if all_max_values:
        max_y = max(all_max_values)
        min_y = min([t for t in all_max_values if t > 0])
        ax_throughput.set_ylim(min_y * 0.5, max_y * 2)

    for entity_idx, entity in enumerate(entities):
        entity_pos = x_pos[entity_idx]

        if ('bare_metal', False) in variant_data.get(entity, {}) and ('bare_metal', True) in variant_data.get(entity, {}):
            base_throughput = variant_data[entity][('bare_metal', False)]
            improved_throughput = variant_data[entity][('bare_metal', True)]

            if base_throughput > 0:
                improvement = improved_throughput / base_throughput
                x_bar_base = entity_pos + 0 * bar_width - 0.3 + bar_width/2
                x_arrow = (x_bar_base) - (bar_width / 2)
                y_start = base_throughput + 0.2 * base_throughput
                y_end = improved_throughput + 0.2 * improved_throughput
                ax_throughput.annotate('', xy=(x_arrow, y_end), xytext=(x_arrow, y_start),
                            arrowprops=dict(arrowstyle='<->', color='darkblue', lw=0.8))
                y_mid = np.sqrt(y_start * y_end)
                ax_throughput.text(x_arrow - bar_width * 0.42, y_mid,
                        f'{improvement:.1f}×',
                        ha='left', va='center',
                        fontsize=ANNOTATION_FONTSIZE_SEP, color='darkblue', rotation=90)

        if ('CVM', False) in variant_data.get(entity, {}) and ('CVM', True) in variant_data.get(entity, {}):
            base_throughput = variant_data[entity][('CVM', False)]
            improved_throughput = variant_data[entity][('CVM', True)]

            if base_throughput > 0:
                improvement = improved_throughput / base_throughput
                x_bar_base = entity_pos + 2 * bar_width - 0.3 + bar_width/2
                x_arrow = (x_bar_base) - (bar_width / 2)
                y_start = base_throughput + 0.2 * base_throughput
                y_end = improved_throughput + 0.2 * improved_throughput
                ax_throughput.annotate('', xy=(x_arrow, y_end), xytext=(x_arrow, y_start),
                            arrowprops=dict(arrowstyle='<->', color='darkblue', lw=0.8))
                y_mid = np.sqrt(y_start * y_end)
                ax_throughput.text(x_arrow - bar_width * 0.42, y_mid,
                        f'{improvement:.1f}×',
                        ha='left', va='center',
                        fontsize=ANNOTATION_FONTSIZE_SEP, color='darkblue', rotation=90)

    ax_throughput.set_xlabel('GDPR Workload', fontsize=LABEL_FONTSIZE_SEP, labelpad=2)
    ax_throughput.set_ylabel('Throughput (ops/s)', fontsize=LABEL_FONTSIZE_SEP, labelpad=2)
    ax_throughput.set_title(f'{db_name.capitalize()} Throughput (Higher is better↑)', color="navy",
                  fontsize=TITLE_FONTSIZE_SEP, pad=3)
    ax_throughput.set_xticks(x_pos)
    ax_throughput.set_xticklabels([e.capitalize() for e in entities], fontsize=TICK_FONTSIZE_SEP)
    ax_throughput.tick_params(axis='x', length=0, pad=2)
    ax_throughput.tick_params(axis='y', labelsize=TICK_FONTSIZE_SEP, pad=2)
    ax_throughput.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()
    
    ax_throughput.legend(loc='upper center', bbox_to_anchor=(0.45, 1.65),
                      ncol=2, fontsize=LEGEND_FONTSIZE_SEP, frameon=True,
                      borderaxespad=0.5, columnspacing=0.45, labelspacing=0.35, borderpad=0.25, handletextpad=0.35, handlelength=1.2)

    output_filename = f'gdpr_metadata_indexes_throughput_only_{db_name}'
    plt.savefig(os.path.join(output_dir, f'{output_filename}.png'), dpi=300, pad_inches=0, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_filename}.pdf'), dpi=300, pad_inches=0, bbox_inches='tight')
    plt.close()

    print(f"  ✓ Throughput-only plot for {db_name}: {output_filename}")

def create_gdpr_queries_metadata_index_performance_plot_separate(data, output_dir, db_name):
    """
    Create separate plot for a single database (Redis or RocksDB).
    Layout: 1 row x 2 columns (Throughput left, Latency right)
    Font sizes increased by 1.
    """
    # Filter to only use encryption=ON data for consistency and specified database
    data_filtered = data[(data['encryption'] == 'ON') & (data['db'] == db_name)].copy()

    if data_filtered.empty:
        print(f"⚠ No data for {db_name}")
        return

    # Increase all font sizes by 1
    FONTSIZE_SEP = FONTSIZE + 1
    TITLE_FONTSIZE_SEP = TITLE_FONTSIZE + 1
    LABEL_FONTSIZE_SEP = LABEL_FONTSIZE + 1
    TICK_FONTSIZE_SEP = TICK_FONTSIZE + 1
    LEGEND_FONTSIZE_SEP = LEGEND_FONTSIZE + 1
    ANNOTATION_FONTSIZE_SEP = ANNOTATION_FONTSIZE + 1

    # Create figure with custom width ratios: 4:1
    fig = plt.figure(figsize=(figwidth_half, 1.2))
    gs = fig.add_gridspec(1, 2, width_ratios=[4, 1], wspace=0.4)

    # Create axes
    ax_throughput = fig.add_subplot(gs[0, 0])
    ax_latency = fig.add_subplot(gs[0, 1])

    # --- Throughput subplot ---
    entities = sorted(data_filtered['entity'].unique())
    n_entities = len(entities)

    variants = [
        ('bare_metal', False, 'Native GDPRuler'),
        ('bare_metal', True, 'Native GDPRuler (w/ indexes)'),
        ('CVM', False, 'GDPRuler'),
        ('CVM', True, 'GDPRuler (w/ indexes)')
    ]

    sample_colors = sns.color_palette("pastel", n_colors=3*len(variants))
    colors = [sample_colors[2], sample_colors[6], sample_colors[5], sample_colors[8]]
    x_pos = np.arange(n_entities)
    bar_width = 0.8 / len(variants)

    variant_data = {}
    all_max_values = []
    for i, (env, has_indexes, label) in enumerate(variants):
        throughputs = []
        throughput_stds = []

        for entity in entities:
            subset = data_filtered[(data_filtered['entity'] == entity) &
                          (data_filtered['environment'] == env) &
                          (data_filtered['metadata_indexes'] == has_indexes)]

            if not subset.empty:
                throughput = subset['throughput'].mean()
                std = subset['throughput'].std()
                all_max_values.append(throughput + std)
                throughputs.append(throughput)
                throughput_stds.append(std)

                if entity not in variant_data:
                    variant_data[entity] = {}
                variant_data[entity][(env, has_indexes)] = throughput
            else:
                throughputs.append(0)
                throughput_stds.append(0)

        bar_positions = x_pos + i * bar_width - 0.3
        bars = ax_throughput.bar(bar_positions, throughputs, bar_width,
                      label=label, color=colors[i],
                      hatch=hatches[i % len(hatches)],
                      alpha=0.8, edgecolor='black')

        ax_throughput.errorbar(bar_positions, throughputs, yerr=throughput_stds,
                    fmt='none', ecolor='black', capsize=1, lw=0.5)

        for j, (bar, throughput) in enumerate(zip(bars, throughputs)):
            if throughput > 0:
                height = bar.get_height()
                ax_throughput.text(bar.get_x() + bar.get_width()/2., height * 1.1,
                        f'{throughput:.0f}',
                        ha='center', va='bottom',
                        fontsize=ANNOTATION_FONTSIZE_SEP, rotation=0)

    ax_throughput.set_yscale('log')
    if all_max_values:
        max_y = max(all_max_values)
        min_y = min([t for t in all_max_values if t > 0])
        ax_throughput.set_ylim(min_y * 0.5, max_y * 2)

    for entity_idx, entity in enumerate(entities):
        entity_pos = x_pos[entity_idx]

        if ('bare_metal', False) in variant_data.get(entity, {}) and ('bare_metal', True) in variant_data.get(entity, {}):
            base_throughput = variant_data[entity][('bare_metal', False)]
            improved_throughput = variant_data[entity][('bare_metal', True)]

            if base_throughput > 0:
                improvement = improved_throughput / base_throughput
                x_bar_base = entity_pos + 0 * bar_width - 0.3 + bar_width/2
                x_arrow = (x_bar_base) - (bar_width / 2)
                y_start = base_throughput + 0.2 * base_throughput
                y_end = improved_throughput + 0.2 * improved_throughput
                ax_throughput.annotate('', xy=(x_arrow, y_end), xytext=(x_arrow, y_start),
                            arrowprops=dict(arrowstyle='<->', color='darkblue', lw=0.8))
                y_mid = np.sqrt(y_start * y_end)
                ax_throughput.text(x_arrow - bar_width * 0.42, y_mid,
                        f'{improvement:.1f}×',
                        ha='left', va='center',
                        fontsize=ANNOTATION_FONTSIZE_SEP, color='darkblue', rotation=90)

        if ('CVM', False) in variant_data.get(entity, {}) and ('CVM', True) in variant_data.get(entity, {}):
            base_throughput = variant_data[entity][('CVM', False)]
            improved_throughput = variant_data[entity][('CVM', True)]

            if base_throughput > 0:
                improvement = improved_throughput / base_throughput
                x_bar_base = entity_pos + 2 * bar_width - 0.3 + bar_width/2
                x_arrow = (x_bar_base) - (bar_width / 2)
                y_start = base_throughput + 0.2 * base_throughput
                y_end = improved_throughput + 0.2 * improved_throughput
                ax_throughput.annotate('', xy=(x_arrow, y_end), xytext=(x_arrow, y_start),
                            arrowprops=dict(arrowstyle='<->', color='darkblue', lw=0.8))
                y_mid = np.sqrt(y_start * y_end)
                ax_throughput.text(x_arrow - bar_width * 0.42, y_mid,
                        f'{improvement:.1f}×',
                        ha='left', va='center',
                        fontsize=ANNOTATION_FONTSIZE_SEP, color='darkblue', rotation=90)

    ax_throughput.set_xlabel('GDPR Workload', fontsize=LABEL_FONTSIZE_SEP, labelpad=1)
    ax_throughput.set_ylabel('Throughput (ops/s)', fontsize=LABEL_FONTSIZE_SEP, labelpad=1)
    ax_throughput.set_title(f'(a) Throughput (Higher is better↑)', color="navy",
                  fontsize=TITLE_FONTSIZE_SEP, pad=3)
    ax_throughput.set_xticks(x_pos)
    ax_throughput.set_xticklabels([e.capitalize() for e in entities], fontsize=TICK_FONTSIZE_SEP)
    ax_throughput.tick_params(axis='x', length=0, pad=1)
    ax_throughput.tick_params(axis='y', labelsize=TICK_FONTSIZE_SEP, pad=1, length=4)
    ax_throughput.grid(True, alpha=0.3, axis='y')

    ax_throughput.legend(loc='upper center', bbox_to_anchor=(0.74, 1.62),
                      ncol=2, fontsize=LEGEND_FONTSIZE_SEP, frameon=True, 
                      borderaxespad=0.5, columnspacing=0.45, labelspacing=0.35, borderpad=0.25, handletextpad=0.35, handlelength=1.2)

    # --- Latency subplot (CVM vs CVM w/ indexes, metadata operations only) ---
    latency_variants = [
        ('CVM', False, 'GDPRuler'),
        ('CVM', True, 'GDPRuler (w/ indexes)')
    ]

    latency_colors = [colors[2], colors[3]]
    latency_hatches = [hatches[2], hatches[3]]

    cvm_data = data_filtered[(data_filtered['environment'] == 'CVM') & (data_filtered['metadata_indexes'] == False)]
    cvm_indexes_data = data_filtered[(data_filtered['environment'] == 'CVM') & (data_filtered['metadata_indexes'] == True)]

    cvm_ops = aggregate_operation_stats(cvm_data)
    cvm_indexes_ops = aggregate_operation_stats(cvm_indexes_data)

    cvm_metadata = {k: v for k, v in cvm_ops.items() if k.upper().endswith('M')}
    cvm_indexes_metadata = {k: v for k, v in cvm_indexes_ops.items() if k.upper().endswith('M')}

    all_ops = set(cvm_metadata.keys()) | set(cvm_indexes_metadata.keys())

    if all_ops:
        gs_lat = ax_latency.get_subplotspec().subgridspec(2, 1, hspace=0.1)
        ax_lat_top = fig.add_subplot(gs_lat[0])
        ax_lat_bottom = fig.add_subplot(gs_lat[1], sharex=ax_lat_top)
        ax_latency.remove()

        op_names = sorted(all_ops, reverse=True)
        x_ops = np.arange(len(op_names))
        bar_width_lat = 0.35

        all_latency_values_with_stds = []

        plotted_bars_bottom = []
        plotted_bars_top = []

        for idx, (env, has_indexes, label) in enumerate(latency_variants):
            means = []
            stds = []

            ops_dict = cvm_indexes_metadata if has_indexes else cvm_metadata

            for op in op_names:
                if op in ops_dict:
                    mean_ms = ops_dict[op]['mean_latency_s'] * 1000
                    std_ms = ops_dict[op]['pooled_std_s'] * 1000
                    means.append(mean_ms)
                    stds.append(std_ms)
                    all_latency_values_with_stds.append((mean_ms, std_ms))
                else:
                    means.append(0)
                    stds.append(0)

            bar_positions = x_ops + idx * bar_width_lat - bar_width_lat/2

            bars_bottom = ax_lat_bottom.bar(bar_positions, means, bar_width_lat,
                                 label=label, color=latency_colors[idx], hatch=latency_hatches[idx],
                                 alpha=0.8, edgecolor='black')
            bars_top = ax_lat_top.bar(bar_positions, means, bar_width_lat,
                                 label=label, color=latency_colors[idx], hatch=latency_hatches[idx],
                                 alpha=0.8, edgecolor='black')

            plotted_bars_bottom.extend(list(zip(bars_bottom, means, stds, [has_indexes]*len(means), bar_positions)))
            plotted_bars_top.extend(list(zip(bars_top, means, stds, [has_indexes]*len(means), bar_positions)))

        if all_latency_values_with_stds:
            max_lat_val = max([m + s for m,s in all_latency_values_with_stds]) if all_latency_values_with_stds else 0
        else:
            max_lat_val = 1000

        y_break_low = 200
        y_break_high = 300

        if max_lat_val < y_break_high:
            ax_lat_bottom.set_ylim(0, max_lat_val * 1.1)
            ax_lat_top.set_visible(False)
            d = 0
        else:
            ax_lat_bottom.set_ylim(0, y_break_low)
            ax_lat_top.set_ylim(y_break_high, max_lat_val * 1.15)

        for idx, (env, has_indexes, label) in enumerate(latency_variants):
            ops_dict = cvm_indexes_metadata if has_indexes else cvm_metadata

            for j, op in enumerate(op_names):
                if op in ops_dict:
                    mean_ms = ops_dict[op]['mean_latency_s'] * 1000
                    std_ms = ops_dict[op]['pooled_std_s'] * 1000

                    bar_x_center = x_ops[j] + idx * bar_width_lat - bar_width_lat/2

                    target_ax = None
                    if mean_ms <= y_break_low:
                        target_ax = ax_lat_bottom
                    elif mean_ms >= y_break_high:
                        target_ax = ax_lat_top

                    if target_ax:
                        bar_obj = None
                        for bar_data in (plotted_bars_bottom if target_ax == ax_lat_bottom else plotted_bars_top):
                            if abs(bar_data[1] - mean_ms) < 1e-6 and \
                               abs(bar_data[4] - (x_ops[j] + idx * bar_width_lat - bar_width_lat/2)) < 1e-6:
                                bar_obj = bar_data[0]
                                break

                        target_ax.errorbar(bar_x_center, mean_ms, yerr=std_ms,
                                          fmt='none', ecolor='black', capsize=1, lw=0.5)

                        if has_indexes:
                            y_position = mean_ms + std_ms 
                            offset = target_ax.get_ylim()[1] * 0.05 

                            target_ax.text(
                                bar_x_center + bar_width_lat/4,
                                y_position + offset,
                                f'{mean_ms:.2f}',
                                ha='center',
                                va='bottom',
                                fontsize=ANNOTATION_FONTSIZE_SEP,
                                rotation=90
                            )

        ax_lat_top.spines['bottom'].set_visible(False)
        ax_lat_bottom.spines['top'].set_visible(False)
        ax_lat_top.tick_params(axis='x', which='both', bottom=False, top=False, labelbottom=False)

        d = .015
        if 'd' in locals() and d > 0:
            kwargs = dict(transform=ax_lat_top.transAxes, color='k', clip_on=False, lw=1.0)
            ax_lat_top.plot((-d, +d), (-d, +d), **kwargs)
            ax_lat_top.plot((1 - d, 1 + d), (-d, +d), **kwargs)
            kwargs.update(transform=ax_lat_bottom.transAxes)
            ax_lat_bottom.plot((-d, +d), (1 - d, 1 + d), **kwargs)
            ax_lat_bottom.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)

        ax_lat_bottom.set_xlabel("")
        fig.text(ax_lat_top.get_position().x0 - 0.11,
                 (ax_lat_bottom.get_position().y0 + ax_lat_top.get_position().y1) / 2,
                 'Avg Latency (ms)', fontsize=LABEL_FONTSIZE_SEP, va='center', rotation='vertical')

        ax_lat_top.set_title(f'(b) Query Latency\n(Lower is better↓)', color="navy",
                          fontsize=TITLE_FONTSIZE_SEP, pad=3)
        ax_lat_bottom.set_xticks(x_ops)
        ax_lat_bottom.set_xticklabels([op.lower() for op in op_names], fontsize=TICK_FONTSIZE_SEP, rotation=25)
        ax_lat_bottom.tick_params(axis='x', length=0, pad=1)

        ax_lat_top.tick_params(axis='y', labelsize=TICK_FONTSIZE_SEP, pad=0, length=3)
        ax_lat_bottom.tick_params(axis='y', labelsize=TICK_FONTSIZE_SEP, pad=0, length=3)

        ax_lat_top.grid(True, alpha=0.3, axis='y')
        ax_lat_bottom.grid(True, alpha=0.3, axis='y')

    else:
        ax_latency.text(0.5, 0.5, 'No metadata\noperations',
                ha='center', va='center', transform=ax_latency.transAxes,
                fontsize=TICK_FONTSIZE_SEP)
        ax_latency.set_xlabel("")
        ax_latency.set_ylabel('Avg Latency (ms)', fontsize=LABEL_FONTSIZE_SEP, labelpad=5)
        ax_latency.set_title(f'{db_name.capitalize()} Query Latency',
                      fontsize=TITLE_FONTSIZE_SEP, pad=3)

    output_filename = f'gdpr_metadata_indexes_performance_{db_name}'
    plt.savefig(os.path.join(output_dir, f'{output_filename}.png'), dpi=300, pad_inches=0, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_filename}.pdf'), dpi=300, pad_inches=0, bbox_inches='tight')
    plt.close()

    print(f"  ✓ Metadata indexes plot for {db_name}: {output_filename}")

def create_gdpr_queries_metadata_index_performance_plot(data, output_dir):
    """
    Create combined plot comparing regular vs metadata indexes variants.
    Layout: 2 rows (Redis top, RocksDB bottom)
    Throughput: Native, Native w/ indexes, CVM, CVM w/ indexes
    Latency: CVM, CVM w/ indexes (both metadata operations)
    """
    databases = sorted(data['db'].unique())
    
    if len(databases) < 2:
        print("⚠ Need both redis and rocksdb data for combined plot")
        return
    
    # Filter to only use encryption=ON data for consistency
    data_filtered = data[data['encryption'] == 'ON'].copy()
    
    # Create figure with custom width ratios: 3:1
    fig = plt.figure(figsize=(figwidth_half, 2.2))
    gs = fig.add_gridspec(2, 2, width_ratios=[4, 1], height_ratios=[1, 1],
                          hspace=0.65, wspace=0.5)
    
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
        df_db = data_filtered[data_filtered['db'] == db_name].copy()
        
        if df_db.empty:
            continue
        
        # --- Throughput subplot --- (No changes here, kept for completeness)
        entities = sorted(df_db['entity'].unique())
        n_entities = len(entities)

        variants = [
            ('bare_metal', False, 'Native GDPRuler'),
            ('bare_metal', True, 'Native GDPRuler (w/ indexes)'),
            ('CVM', False, 'GDPRuler'),
            ('CVM', True, 'GDPRuler (w/ indexes)')
        ]

        sample_colors = sns.color_palette("pastel", n_colors=3*len(variants))
        colors = [sample_colors[2], sample_colors[6], sample_colors[5], sample_colors[8]]
        x_pos = np.arange(n_entities)
        bar_width = 0.8 / len(variants)

        variant_data = {}
        all_max_values = []
        for i, (env, has_indexes, label) in enumerate(variants):
            throughputs = []
            throughput_stds = []
            
            for entity in entities:
                subset = df_db[(df_db['entity'] == entity) &
                              (df_db['environment'] == env) &
                              (df_db['metadata_indexes'] == has_indexes)]
                
                if not subset.empty:
                    throughput = subset['throughput'].mean()
                    std = subset['throughput'].std()
                    all_max_values.append(throughput + std)
                    throughputs.append(throughput)
                    throughput_stds.append(std)
                    
                    if entity not in variant_data:
                        variant_data[entity] = {}
                    variant_data[entity][(env, has_indexes)] = throughput
                else:
                    throughputs.append(0)
                    throughput_stds.append(0)
            
            bar_positions = x_pos + i * bar_width - 0.3
            bars = ax_throughput.bar(bar_positions, throughputs, bar_width,
                          label=label, color=colors[i],
                          hatch=hatches[i % len(hatches)],
                          alpha=0.8, edgecolor='black')
            
            ax_throughput.errorbar(bar_positions, throughputs, yerr=throughput_stds,
                        fmt='none', ecolor='black', capsize=1, lw=0.5)
            
            for j, (bar, throughput) in enumerate(zip(bars, throughputs)):
                if throughput > 0:
                    height = bar.get_height()
                    ax_throughput.text(bar.get_x() + bar.get_width()/2., height * 1.1,
                            f'{throughput:.0f}',
                            ha='center', va='bottom',
                            fontsize=ANNOTATION_FONTSIZE, rotation=0)

        ax_throughput.set_yscale('log')
        if all_max_values:
            max_y = max(all_max_values)
            min_y = min([t for t in all_max_values if t > 0])
            ax_throughput.set_ylim(min_y * 0.5, max_y * 2)

        for entity_idx, entity in enumerate(entities):
            entity_pos = x_pos[entity_idx]
            
            if ('bare_metal', False) in variant_data.get(entity, {}) and ('bare_metal', True) in variant_data.get(entity, {}):
                base_throughput = variant_data[entity][('bare_metal', False)]
                improved_throughput = variant_data[entity][('bare_metal', True)]
                
                if base_throughput > 0:
                    improvement = improved_throughput / base_throughput
                    x_bar_base = entity_pos + 0 * bar_width - 0.3 + bar_width/2
                    x_arrow = (x_bar_base) - (bar_width / 2)
                    y_start = base_throughput + 0.2 * base_throughput
                    y_end = improved_throughput + 0.2 * improved_throughput
                    ax_throughput.annotate('', xy=(x_arrow, y_end), xytext=(x_arrow, y_start),
                                arrowprops=dict(arrowstyle='<->', color='darkblue', lw=0.8))
                    y_mid = np.sqrt(y_start * y_end)
                    ax_throughput.text(x_arrow - bar_width * 0.42, y_mid,
                            f'{improvement:.1f}×',
                            ha='left', va='center',
                            fontsize=ANNOTATION_FONTSIZE, color='darkblue', rotation=90)
            
            if ('CVM', False) in variant_data.get(entity, {}) and ('CVM', True) in variant_data.get(entity, {}):
                base_throughput = variant_data[entity][('CVM', False)]
                improved_throughput = variant_data[entity][('CVM', True)]
                
                if base_throughput > 0:
                    improvement = improved_throughput / base_throughput
                    x_bar_base = entity_pos + 2 * bar_width - 0.3 + bar_width/2
                    x_arrow = (x_bar_base) - (bar_width / 2)
                    y_start = base_throughput + 0.2 * base_throughput
                    y_end = improved_throughput + 0.2 * improved_throughput
                    ax_throughput.annotate('', xy=(x_arrow, y_end), xytext=(x_arrow, y_start),
                                arrowprops=dict(arrowstyle='<->', color='darkblue', lw=0.8))
                    y_mid = np.sqrt(y_start * y_end)
                    ax_throughput.text(x_arrow - bar_width * 0.42, y_mid,
                            f'{improvement:.1f}×',
                            ha='left', va='center',
                            fontsize=ANNOTATION_FONTSIZE, color='darkblue', rotation=90)

        ax_throughput.set_xlabel('GDPR Workload', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax_throughput.set_ylabel('Throughput (ops/s)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax_throughput.set_title(f'{label_left} Throughput (Higher is better↑)', color="navy",
                      fontsize=TITLE_FONTSIZE, pad=3)
        ax_throughput.set_xticks(x_pos)
        ax_throughput.set_xticklabels([e.capitalize() for e in entities], fontsize=TICK_FONTSIZE)
        ax_throughput.tick_params(axis='x', length=0, pad=2)
        ax_throughput.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax_throughput.grid(True, alpha=0.3, axis='y')

        if db_name == 'redis':
            ax_throughput.legend(loc='upper center', bbox_to_anchor=(0.8, 1.85),
                              ncol=2, fontsize=LEGEND_FONTSIZE, frameon=True)
        
        # --- Latency subplot (CVM vs CVM w/ indexes, metadata operations only) ---
        latency_variants = [
            ('CVM', False, 'GDPRuler'),
            ('CVM', True, 'GDPRuler (w/ indexes)')
        ]
        
        latency_colors = [colors[2], colors[3]]
        latency_hatches = [hatches[2], hatches[3]]
        
        cvm_data = df_db[(df_db['environment'] == 'CVM') & (df_db['metadata_indexes'] == False)]
        cvm_indexes_data = df_db[(df_db['environment'] == 'CVM') & (df_db['metadata_indexes'] == True)]
        
        cvm_ops = aggregate_operation_stats(cvm_data)
        cvm_indexes_ops = aggregate_operation_stats(cvm_indexes_data)
        
        cvm_metadata = {k: v for k, v in cvm_ops.items() if k.upper().endswith('M')}
        cvm_indexes_metadata = {k: v for k, v in cvm_indexes_ops.items() if k.upper().endswith('M')}
        
        all_ops = set(cvm_metadata.keys()) | set(cvm_indexes_metadata.keys())
        
        if all_ops:
            gs_lat = ax_latency.get_subplotspec().subgridspec(2, 1, hspace=0.1)
            ax_lat_top = fig.add_subplot(gs_lat[0])
            ax_lat_bottom = fig.add_subplot(gs_lat[1], sharex=ax_lat_top)
            ax_latency.remove()

            op_names = sorted(all_ops, reverse=True)
            x_ops = np.arange(len(op_names))
            bar_width_lat = 0.35
            
            all_latency_values_with_stds = [] # Store mean and std for all operations

            # Data structures to store plotted bar objects for annotations/error bars later
            plotted_bars_bottom = []
            plotted_bars_top = []
            
            # First pass: Plot bars on both axes
            for idx, (env, has_indexes, label) in enumerate(latency_variants):
                means = []
                stds = []
                
                ops_dict = cvm_indexes_metadata if has_indexes else cvm_metadata
                
                for op in op_names:
                    if op in ops_dict:
                        mean_ms = ops_dict[op]['mean_latency_s'] * 1000
                        std_ms = ops_dict[op]['pooled_std_s'] * 1000
                        means.append(mean_ms)
                        stds.append(std_ms)
                        all_latency_values_with_stds.append((mean_ms, std_ms))
                    else:
                        means.append(0)
                        stds.append(0)
                
                bar_positions = x_ops + idx * bar_width_lat - bar_width_lat/2
                
                # Plot on both axes, but store the bar objects separately
                bars_bottom = ax_lat_bottom.bar(bar_positions, means, bar_width_lat,
                                     label=label, color=latency_colors[idx], hatch=latency_hatches[idx],
                                     alpha=0.8, edgecolor='black')
                bars_top = ax_lat_top.bar(bar_positions, means, bar_width_lat,
                                     label=label, color=latency_colors[idx], hatch=latency_hatches[idx],
                                     alpha=0.8, edgecolor='black')
                
                plotted_bars_bottom.extend(list(zip(bars_bottom, means, stds, [has_indexes]*len(means), bar_positions)))
                plotted_bars_top.extend(list(zip(bars_top, means, stds, [has_indexes]*len(means), bar_positions)))


            # Determine appropriate y-limits based on all data
            if all_latency_values_with_stds:
                max_lat_val = max([m + s for m,s in all_latency_values_with_stds]) if all_latency_values_with_stds else 0
            else:
                max_lat_val = 1000 # Default if no data

            y_break_low = 200
            y_break_high = 300
            
            # Ensure the broken axis range covers the max values if they are within the break
            if max_lat_val < y_break_high: # No need for a break if max is below high break point
                ax_lat_bottom.set_ylim(0, max_lat_val * 1.1)
                ax_lat_top.set_visible(False) # Hide the top axis
                # Remove break marks if no break
                d = 0
            else:
                ax_lat_bottom.set_ylim(0, y_break_low)
                ax_lat_top.set_ylim(y_break_high, max_lat_val * 1.15) # Added headroom for annotation

            # Second pass: Add annotations and error bars on the correct axis
            for idx, (env, has_indexes, label) in enumerate(latency_variants):
                ops_dict = cvm_indexes_metadata if has_indexes else cvm_metadata
                
                for j, op in enumerate(op_names):
                    if op in ops_dict:
                        mean_ms = ops_dict[op]['mean_latency_s'] * 1000
                        std_ms = ops_dict[op]['pooled_std_s'] * 1000
                        
                        bar_x_center = x_ops[j] + idx * bar_width_lat - bar_width_lat/2
                        
                        target_ax = None
                        if mean_ms <= y_break_low:
                            target_ax = ax_lat_bottom
                        elif mean_ms >= y_break_high:
                            target_ax = ax_lat_top
                        
                        if target_ax:
                            # Find the specific bar object on the target axis
                            bar_obj = None
                            for bar_data in (plotted_bars_bottom if target_ax == ax_lat_bottom else plotted_bars_top):
                                # bar_data: (bar_object, mean_val, std_val, has_indexes, bar_position_x_start)
                                if abs(bar_data[1] - mean_ms) < 1e-6 and \
                                   abs(bar_data[4] - (x_ops[j] + idx * bar_width_lat - bar_width_lat/2)) < 1e-6:
                                    bar_obj = bar_data[0]
                                    break
                            
                            # First, add the error bar on the target axis
                            target_ax.errorbar(bar_x_center, mean_ms, yerr=std_ms,
                                              fmt='none', ecolor='black', capsize=1, lw=0.5)

                            # Next, annotate ONLY the indexed variant, positioned above the error bar
                            if has_indexes:
                                # The new Y position is the top of the error bar
                                y_position = mean_ms + std_ms 
                                
                                # Add a small vertical offset so the text doesn't touch the error bar
                                # This offset is a small fraction of the axis's visible height
                                offset = target_ax.get_ylim()[1] * 0.05 

                                target_ax.text(
                                    bar_x_center + bar_width_lat/4,# Use the bar's center X-coordinate
                                    y_position + offset,           # Position text above the error bar
                                    f'{mean_ms:.2f}',              # The text to display
                                    ha='center',                   # Horizontally align to the center
                                    va='bottom',                   # Vertically align to the bottom of the text
                                    fontsize=ANNOTATION_FONTSIZE,
                                    rotation=90
                                )

            # --- Broken axis visual styling ---
            ax_lat_top.spines['bottom'].set_visible(False)
            ax_lat_bottom.spines['top'].set_visible(False)
            ax_lat_top.tick_params(axis='x', which='both', bottom=False, top=False, labelbottom=False)
            
            d = .015
            # Only draw break marks if a break is actually present
            if 'd' in locals() and d > 0: # Check if d was set to 0 to skip drawing
                kwargs = dict(transform=ax_lat_top.transAxes, color='k', clip_on=False, lw=1.0)
                ax_lat_top.plot((-d, +d), (-d, +d), **kwargs)
                ax_lat_top.plot((1 - d, 1 + d), (-d, +d), **kwargs)
                kwargs.update(transform=ax_lat_bottom.transAxes)
                ax_lat_bottom.plot((-d, +d), (1 - d, 1 + d), **kwargs)
                ax_lat_bottom.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)

            # --- Labels and titles ---
            ax_lat_bottom.set_xlabel("")
            # Position the shared Y-label in the middle of the combined axes
            fig.text(ax_lat_top.get_position().x0 - 0.11,
                     (ax_lat_bottom.get_position().y0 + ax_lat_top.get_position().y1) / 2,
                     'Avg Latency (ms)', fontsize=LABEL_FONTSIZE, va='center', rotation='vertical')

            ax_lat_top.set_title(f'{label_right} Query Latency\n(Lower is better↓)', color="navy",
                              fontsize=TITLE_FONTSIZE, pad=3)
            ax_lat_bottom.set_xticks(x_ops)
            ax_lat_bottom.set_xticklabels([op.lower() for op in op_names], fontsize=TICK_FONTSIZE, rotation=25)
            ax_lat_bottom.tick_params(axis='x', length=0, pad=2)
            
            ax_lat_top.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
            ax_lat_bottom.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)

            ax_lat_top.grid(True, alpha=0.3, axis='y')
            ax_lat_bottom.grid(True, alpha=0.3, axis='y')

        else:
            ax_latency.text(0.5, 0.5, 'No metadata\noperations',
                    ha='center', va='center', transform=ax_latency.transAxes,
                    fontsize=TICK_FONTSIZE)
            ax_latency.set_xlabel("")
            ax_latency.set_ylabel('Avg Latency (ms)', fontsize=LABEL_FONTSIZE, labelpad=2)
            ax_latency.set_title(f'{label_right} Query Latency',
                          fontsize=TITLE_FONTSIZE, pad=3)
    
    fig.text(-0.01, 0.75, 'Redis', fontsize=LABEL_FONTSIZE + 1, rotation=90,
             verticalalignment='center', horizontalalignment='center', weight='bold')
    fig.text(-0.01, 0.25, 'RocksDB', fontsize=LABEL_FONTSIZE + 1, rotation=90,
             verticalalignment='center', horizontalalignment='center', weight='bold')
    
    output_filename = 'gdpr_metadata_indexes_performance'
    plt.savefig(os.path.join(output_dir, f'{output_filename}.png'), dpi=300, bbox_inches='tight', pad_inches=0)
    plt.savefig(os.path.join(output_dir, f'{output_filename}.pdf'), dpi=300, bbox_inches='tight', pad_inches=0)
    plt.close()
    
    print(f"  ✓ Metadata indexes plot: {output_filename}")
    print_gdpr_workload_statistics(data)

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
            ('CVM', 'OFF', 'GDPRuler (w/o Encr)'),
            ('CVM', 'ON', 'GDPRuler (w/ Encr)')
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
            ax_throughput.legend(loc='upper center', bbox_to_anchor=(0.8, 1.72), 
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
            ax_latency.set_title(f'{label_right} Query Latency\n(Lower is better↓)',  color="navy",
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
            ax_latency.set_title(f'{label_right} Query Latency', 
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
        
        # Plot 3: Per-Query Latency by entity
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
        
        fig.suptitle(f'{db_name.capitalize()} - Per-Query Latency by Entity', 
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
    print(f"Metadata indexes variants: {data['metadata_indexes'].unique()}")
    
    
    # Filter data for regular plots (no metadata indexes)
    data_without_indexes = data[data['metadata_indexes'] == False].copy()
    
    print_statistics(data)
    
    print(f"\n{'='*80}")
    print("GENERATING PLOTS")
    print(f"{'='*80}\n")
    
    # Create individual plots for each database
    # for db in sorted(data['db'].unique()):
        # create_performance_per_db_plot(data_without_indexes, db, args.output_dir)
    
    # Create combined plot with both databases
    # create_gdpr_queries_performance_plot(data_without_indexes, args.output_dir)
    
    # create_analytical_plots(data_without_indexes, args.output_dir)
    
    # Create metadata indexes comparison plot if that data exists
    if data['metadata_indexes'].any():
        print("\nGenerating metadata indexes comparison plot...")
        create_gdpr_queries_metadata_index_performance_plot(data, args.output_dir)
        create_gdpr_queries_metadata_index_performance_plot_separate(data, args.output_dir, 'redis')
        create_gdpr_queries_metadata_index_performance_plot_separate(data, args.output_dir, 'rocksdb')
        create_gdpr_queries_metadata_index_throughput_only_plot(data, args.output_dir, 'redis')
        create_gdpr_queries_metadata_index_throughput_only_plot(data, args.output_dir, 'rocksdb')
    
    print(f"\n{'='*80}")
    print(f"✓ All plots saved to: {args.output_dir}")
    print(f"{'='*80}\n")

if __name__ == "__main__":
    main()
