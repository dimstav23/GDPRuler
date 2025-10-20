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

FONTSIZE = 6
TITLE_FONTSIZE = FONTSIZE
LABEL_FONTSIZE = FONTSIZE
TICK_FONTSIZE = FONTSIZE - 1
LEGEND_FONTSIZE = FONTSIZE
ANNOTATION_FONTSIZE = FONTSIZE / 2
# hatches = ["", "o", "*", ".", "//", "-", "\\", ".", "o-", "*-"]
hatches = ['', '///', '\\\\\\', 'xxx', '...', '+++', '', '///', '\\\\\\', 'xxx', '...', '+++']

workload_size = {
    "small": 1_000,
    "medium": 1_000_000,
    "large": 10_000_000
}

variant_mapping = {
    "direct_bare_metal"           : "Native KVS",
    "passthrough_bare_metal"      : "Native passthrough",
    "gdpr_bare_metal"             : "Native GDPRuler",
    "direct_CVM"                  : "CVM KVS",
    "passthrough_CVM"             : "CVM passthrough",
    "gdpr_CVM"                    : "GDPRuler",
}

def load_data_from_directory(input_dir):
    pattern = r"(?P<controller>\w+(?:_\w+)?)-(?P<workload_type>[\w_]+)-encryption_(?P<encryption>\w+)-logging_(?P<logging>\w+)-connection_(?P<connection>\w+)\.csv"
    data = []
    
    for filename in os.listdir(input_dir):
        if filename.endswith(".csv") and "gdpr_queries" not in filename: # exclude gdpr workload results
            match = re.match(pattern, filename)
            if match:
                # Skip files with logging=ON
                if match.group("logging") == "ON":
                    continue
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

def filter_data_for_ycsb_performance_plot(df, include_tcp = False):
    """Filter data for paper-performance plots:
    - Ignore passthrough variants
    - Consider only UNIX connection for GDPRuler variants
    """
    # Only keep variants without 'passthrough' in their names
    df_filtered = df[~df['variant'].str.contains('passthrough')].copy()
    
    # For GDPRuler variants, keep only UNIX connection if include_tcp is false
    if not include_tcp:
        mask_gdpruler = df_filtered['variant'].str.contains('gdpr')
        df_filtered = df_filtered[~(mask_gdpruler & (df_filtered['connection'] != 'UNIX'))]

    return df_filtered

def prepare_plot_data(df_subset, metric, group_by):
    # Pick the column that contains the raw numbers
    if metric == 'latency':
        col = f'avg_{metric} (s)'
    else:  # throughput
        col = 'throughput'

    g = df_subset.groupby(group_by)[col]
    mean_vals = g.mean()
    std_vals = g.std(ddof=0)  # population std (use ddof=1 for sample std)

    # Unit conversions
    if metric == 'latency':
        mean_vals *= 1_000_000  # s → μs
        std_vals *= 1_000_000
    else:  # throughput
        mean_vals /= 1_000  # ops → kops
        std_vals /= 1_000

    return mean_vals, std_vals

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

def print_ycsb_statistics(data_filtered, variants):
    """Print comprehensive statistics with TRUE min-max ranges across all measurements"""
    
    print("\n" + "="*80)
    print("YCSB PERFORMANCE STATISTICS (For Paper Placeholders)")
    print("="*80)
    
    # Identify variants
    baseline_variants = [v for v in variants if v.startswith('direct_bare_metal')]
    gdpruler_cvm_variants = [v for v in variants if v.startswith('gdpr_CVM')]
    cvm_variants = [v for v in variants if v.startswith('direct_CVM')]
    
    if not baseline_variants or not gdpruler_cvm_variants:
        print("\nERROR: Could not find required variants")
        return
    
    # Select specific variants
    baseline = [v for v in baseline_variants if '-encr-' in v and '-unix' in v]
    baseline = baseline[0] if baseline else baseline_variants[0]
    
    gdpruler_encr = [v for v in gdpruler_cvm_variants if '-encr-' in v and '-unix' in v]
    gdpruler_encr = gdpruler_encr[0] if gdpruler_encr else gdpruler_cvm_variants[0]
    
    gdpruler_no_encr = [v for v in gdpruler_cvm_variants if '-no_encr-' in v and '-unix' in v]
    gdpruler_no_encr = gdpruler_no_encr[0] if gdpruler_no_encr else None
    
    cvm_baseline = [v for v in cvm_variants if '-encr-' in v and '-unix' in v]
    cvm_baseline = cvm_baseline[0] if cvm_baseline else (cvm_variants[0] if cvm_variants else None)
    
    print("\nUsing for analysis:")
    print(f"  Baseline: {baseline}")
    print(f"  GDPRuler: {gdpruler_encr}")
    print(f"  GDPRuler (no encr): {gdpruler_no_encr}")
    print(f"  CVM: {cvm_baseline}")
    
    dbs = ['redis', 'rocksdb']
    
    # Collect ALL percentages for true ranges
    all_overall_pcts = []
    all_workloadc_pcts = []
    all_workloada_pcts = []
    
    results = {
        'overall': {},
        'workload_c': {},
        'workload_a': {},
        'max_threads': {},
        'cvm_overhead': {},
        'encryption_overhead': {}
    }
    
    # Calculate per-workload, per-DB percentages
    print("\n" + "="*80)
    print("DETAILED PERFORMANCE BY WORKLOAD AND DB (1 thread)")
    print("="*80)
    
    for db in dbs:
        print(f"\n{db.upper()}:")
        results['overall'][db] = {}
        results['workload_c'][db] = []
        results['workload_a'][db] = []
        
        # Get all workloads for this DB
        workloads = sorted(data_filtered[data_filtered['db'] == db]['workload'].unique())
        
        for wl in workloads:
            df_wl = data_filtered[(data_filtered['db'] == db) & 
                                  (data_filtered['workload'] == wl) &
                                  (data_filtered['n_clients'] == 1)]
            baseline_val = df_wl[df_wl['variant'] == baseline]['throughput'].mean()
            gdpruler_val = df_wl[df_wl['variant'] == gdpruler_encr]['throughput'].mean()
            
            if not pd.isna(baseline_val) and not pd.isna(gdpruler_val) and baseline_val > 0:
                pct = (gdpruler_val / baseline_val * 100)
                all_overall_pcts.append(pct)
                results['overall'][db][wl] = pct
                
                print(f"  {wl}: {gdpruler_val:.1f} / {baseline_val:.1f} = {pct:.1f}%")
                
                # Track workload-specific
                if wl == 'workloadc':
                    all_workloadc_pcts.append(pct)
                    results['workload_c'][db] = pct
                elif wl == 'workloada':
                    all_workloada_pcts.append(pct)
                    results['workload_a'][db] = pct
    
    # Max threads
    thread_counts = sorted(data_filtered['n_clients'].unique())
    max_threads = max(thread_counts)
    print(f"\n" + "="*80)
    print(f"PERFORMANCE AT {max_threads} THREADS")
    print("="*80)
    
    for db in dbs:
        print(f"\n{db.upper()}:")
        results['max_threads'][db] = {}
        
        for workload in ['workloadc', 'workloada']:
            df_max = data_filtered[(data_filtered['db'] == db) & 
                                 (data_filtered['workload'] == workload) &
                                 (data_filtered['n_clients'] == max_threads)]
            baseline_max = df_max[df_max['variant'] == baseline]['throughput'].mean()
            gdpruler_max = df_max[df_max['variant'] == gdpruler_encr]['throughput'].mean()
            
            if not pd.isna(baseline_max) and not pd.isna(gdpruler_max):
                results['max_threads'][db][workload] = {
                    'baseline': baseline_max,
                    'gdpruler': gdpruler_max
                }
                print(f"  {workload}: GDPRuler={gdpruler_max:.1f}, Baseline={baseline_max:.1f} kops/sec")
    
    # CVM overhead
    if cvm_baseline:
        print("\n" + "="*80)
        print("CVM OVERHEAD")
        print("="*80)
        for db in dbs:
            df_db = data_filtered[(data_filtered['db'] == db) & (data_filtered['n_clients'] == 1)]
            baseline_avg = df_db[df_db['variant'] == baseline]['throughput'].mean()
            cvm_avg = df_db[df_db['variant'] == cvm_baseline]['throughput'].mean()
            
            if not pd.isna(baseline_avg) and not pd.isna(cvm_avg) and baseline_avg > 0:
                overhead = ((baseline_avg - cvm_avg) / baseline_avg * 100)
                results['cvm_overhead'][db] = overhead
                print(f"{db.upper()}: {overhead:.1f}% overhead")

    # ========== GDPRULER vs CVM OVERHEAD ==========
    if cvm_baseline:
        print("\n" + "="*100)
        print("GDPRULER OVERHEAD ON TOP OF CVM (GDPRuler-CVM vs CVM)")
        print("="*100)
        
        gdpr_on_cvm_overheads = []
        print(f"\n{'DB':<12} {'CVM':>12} {'GDPRuler':>12} {'Overhead':>12} {'Range':>15}")
        print("-" * 65)
        
        for db in dbs:
            df = data_filtered[(data_filtered['db'] == db) & (data_filtered['n_clients'] == 1)]
            cvm_vals = df[df['variant'] == cvm_baseline]['throughput'].values
            gdpr_vals = df[df['variant'] == gdpruler_encr]['throughput'].values
            
            if len(cvm_vals) > 0 and len(gdpr_vals) > 0:
                overheads = []
                for c_val in cvm_vals:
                    for g_val in gdpr_vals:
                        if c_val > 0:
                            overheads.append(((c_val - g_val) / c_val * 100))
                
                if overheads:
                    avg_oh = sum(overheads) / len(overheads)
                    min_oh = min(overheads)
                    max_oh = max(overheads)
                    gdpr_on_cvm_overheads.extend(overheads)
                    
                    print(f"{db.upper():<12} {cvm_vals.mean():>10.1f} {gdpr_vals.mean():>10.1f} {avg_oh:>10.1f}% {min_oh:>6.1f}-{max_oh:<6.1f}%")
        
        if gdpr_on_cvm_overheads:
            print(f"\nGDPR Layer Overhead on CVM: {sum(gdpr_on_cvm_overheads)/len(gdpr_on_cvm_overheads):.1f}% "
                  f"(range: {min(gdpr_on_cvm_overheads):.1f}%-{max(gdpr_on_cvm_overheads):.1f}%)")
            print("→ This shows the incremental cost of adding GDPR compliance to an existing CVM")

    # Encryption overhead
    if gdpruler_no_encr:
        print("\n" + "="*80)
        print("ENCRYPTION OVERHEAD")
        print("="*80)
        for db in dbs:
            df_db = data_filtered[(data_filtered['db'] == db) & (data_filtered['n_clients'] == 1)]
            no_encr_avg = df_db[df_db['variant'] == gdpruler_no_encr]['throughput'].mean()
            with_encr_avg = df_db[df_db['variant'] == gdpruler_encr]['throughput'].mean()
            
            if not pd.isna(no_encr_avg) and not pd.isna(with_encr_avg) and no_encr_avg > 0:
                overhead = ((no_encr_avg - with_encr_avg) / no_encr_avg * 100)
                results['encryption_overhead'][db] = overhead
                print(f"{db.upper()}: {overhead:.1f}% overhead")
    
    # Print summary with TRUE MIN-MAX ranges
    print("\n" + "="*80)
    print("SUMMARY - PAPER PLACEHOLDERS (True Min-Max Ranges)")
    print("="*80)
    
    print("\n[X, Y] GDPRuler performance by DB (average across all workloads at 1 thread):")
    for db in dbs:
        if db in results['overall'] and results['overall'][db]:
            avg_pct = sum(results['overall'][db].values()) / len(results['overall'][db])
            print(f"  {db.upper()}: {avg_pct:.1f}%")
    
    if all_overall_pcts:
        print(f"\n  TRUE RANGE across all workloads & DBs: {min(all_overall_pcts):.1f}%-{max(all_overall_pcts):.1f}%")
        print(f"  → Use this for the overall range if you want min-max")
    
    if all_workloadc_pcts:
        print(f"\nWorkload C (read-heavy) range:")
        for db in dbs:
            if db in results['workload_c']:
                print(f"  {db.upper()}: {results['workload_c'][db]:.1f}%")
        print(f"  TRUE RANGE: {min(all_workloadc_pcts):.1f}%-{max(all_workloadc_pcts):.1f}%")
    
    if all_workloada_pcts:
        print(f"\nWorkload A (write-heavy) range:")
        for db in dbs:
            if db in results['workload_a']:
                print(f"  {db.upper()}: {results['workload_a'][db]:.1f}%")
        print(f"  TRUE RANGE: {min(all_workloada_pcts):.1f}%-{max(all_workloada_pcts):.1f}%")
    
    if results['max_threads']:
        print(f"\n[{max_threads} threads] Absolute values:")
        for db in dbs:
            if db in results['max_threads']:
                print(f"  {db.upper()}:")
                for wl in ['workloadc', 'workloada']:
                    if wl in results['max_threads'][db]:
                        g = results['max_threads'][db][wl]['gdpruler']
                        b = results['max_threads'][db][wl]['baseline']
                        print(f"    {wl}: GDPRuler={g:.1f}, Baseline={b:.1f} kops/sec")
    
    if results['cvm_overhead']:
        vals = list(results['cvm_overhead'].values())
        print(f"\n[E-F] CVM overhead:")
        for db, val in results['cvm_overhead'].items():
            print(f"  {db.upper()}: {val:.1f}%")
        print(f"  RANGE: {min(vals):.1f}%-{max(vals):.1f}%")
    
    if results['encryption_overhead']:
        vals = list(results['encryption_overhead'].values())
        print(f"\n[G-H] Encryption overhead:")
        for db, val in results['encryption_overhead'].items():
            print(f"  {db.upper()}: {val:.1f}%")
        print(f"  RANGE: {min(vals):.1f}%-{max(vals):.1f}%")
    
    print("\n" + "="*80)

def create_single_thread_comparison_plot(data, output_dir, include_tcp=False):
    """
    Plot 1: Redis and RocksDB side-by-side for 1 thread across all workloads
    Layout: 1 row x 2 columns (Redis left, RocksDB right)
    Font sizes increased by 1, height = 1.2
    """
    # Filter data
    data_filtered = filter_data_for_ycsb_performance_plot(data, include_tcp=include_tcp)
    data_1thread = data_filtered[data_filtered['n_clients'] == 1]

    # Increase font sizes by 1
    FONTSIZE_SEP = FONTSIZE + 1
    TITLE_FONTSIZE_SEP = TITLE_FONTSIZE + 1
    LABEL_FONTSIZE_SEP = LABEL_FONTSIZE + 1
    TICK_FONTSIZE_SEP = TICK_FONTSIZE + 1
    LEGEND_FONTSIZE_SEP = LEGEND_FONTSIZE + 1

    fig, axes = plt.subplots(1, 2, figsize=(figwidth_full, 1.2))

    dbs = ['redis', 'rocksdb']
    titles = ['(a) Redis (Higher is better↑)', '(b) RocksDB (Higher is better↑)']

    # Get sorted variants and setup colors/hatches
    variants = sort_variants(data_1thread['variant'].unique())
    colors = sns.color_palette("pastel", n_colors=len(variants))

    workloads = sorted(data_1thread['workload'].unique())
    x_workloads = range(len(workloads))
    width = 0.8 / len(variants)

    for i, db in enumerate(dbs):
        ax = axes[i]
        df_db = data_1thread[data_1thread['db'] == db]

        for j, variant in enumerate(variants):
            df_var = df_db[df_db['variant'] == variant]
            if not df_var.empty:
                values, errors = prepare_plot_data(df_var, 'throughput', 'workload')
                offset = width * j - 0.4 + width / 2
                bar_positions = [xi + offset for xi in x_workloads]
                ax.bar(bar_positions, values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
                if errors is not None:
                    ax.errorbar(bar_positions, values, yerr=errors,
                                fmt='none', ecolor='black', capsize=1, lw=0.5)

        ax.set_xticks(x_workloads)
        ax.set_xticklabels([w[-1].upper() for w in workloads], fontsize=TICK_FONTSIZE_SEP)
        ax.tick_params(axis='x', length=0, pad=2)
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE_SEP, pad=1)
        ax.set_xlabel('YCSB Workload', fontsize=LABEL_FONTSIZE_SEP, labelpad=0)
        ax.set_ylabel('Throughput (kops/s)', fontsize=LABEL_FONTSIZE_SEP, labelpad=0)
        ax.set_title(titles[i], fontsize=TITLE_FONTSIZE_SEP, color="navy", pad=3)
        ax.grid(True, alpha=0.3, axis='y')

    # Create legend
    handles, labels = [], []
    for j, variant in enumerate(variants):
        handles.append(plt.Rectangle((0,0),1,1, facecolor=colors[j], alpha=0.8, 
                                   hatch=hatches[j % len(hatches)], edgecolor='black'))
        baseline_type = variant.split('-')[0]
        label_text = variant_mapping.get(baseline_type, baseline_type)

        if 'no_encr' in variant and "direct_" not in variant:
            label_text += " (w/o Encr)"
        # else:
        #     label_text += " (w/ Encr)"

        if include_tcp:
            if 'tcp' in variant:
                label_text += " (TCP)"
            else:
                label_text += " (UNIX)"

        labels.append(label_text)

    fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 1.12), 
              ncol=len(variants), fontsize=LEGEND_FONTSIZE_SEP, frameon=True,
              borderaxespad=0.5, columnspacing=0.45, labelspacing=0.35, borderpad=0.25, handletextpad=0.35, handlelength=1.2)

    plt.tight_layout()

    os.makedirs(output_dir, exist_ok=True)
    output_file = "ycsb_single_thread_comparison" if not include_tcp else "ycsb_single_thread_comparison_tcp"
    plt.savefig(os.path.join(output_dir, f'{output_file}.png'), dpi=300, pad_inches=0, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_file}.pdf'), pad_inches=0, bbox_inches='tight')
    plt.close()

    print(f"  ✓ Single thread comparison plot: {output_file}")

def create_workload_scalability_plot(data, output_dir, workload='workloada', include_tcp=False):
    """
    Plot 2: Redis and RocksDB side-by-side for Workload scalability
    Layout: 1 row x 2 columns (Redis left, RocksDB right)
    Font sizes increased by 1, height = 1.2
    """
    # Filter data
    data_filtered = filter_data_for_ycsb_performance_plot(data, include_tcp=include_tcp)
    data_workload = data_filtered[data_filtered['workload'] == workload]

    # Increase font sizes by 1
    FONTSIZE_SEP = FONTSIZE + 1
    TITLE_FONTSIZE_SEP = TITLE_FONTSIZE + 1
    LABEL_FONTSIZE_SEP = LABEL_FONTSIZE + 1
    TICK_FONTSIZE_SEP = TICK_FONTSIZE + 1
    LEGEND_FONTSIZE_SEP = LEGEND_FONTSIZE + 1

    fig, axes = plt.subplots(1, 2, figsize=(figwidth_full, 1.2))

    workload_mapping = {
        "workloada": "Workload A (50/50 R/W)",
        "workloadb": "Workload B (95/5 R/W)",
        "workloadc": "Workload C (100/0 R/W)",
        "workloadd": "Workload D (95/5 R/I)",
        "workloadf": "Workload F (100 RMW)",
    }
    
    # Print statistics header
    print("\n" + "="*90)
    print(f"SCALABILITY STATISTICS FOR {workload.upper()}")
    print(f"Description: {workload_mapping[workload]}")
    print("="*90)
    
    dbs = ['redis', 'rocksdb']
    titles = [f"(a) Redis - {workload_mapping[workload]} (Higher is better↑)", 
              f"(b) RocksDB - {workload_mapping[workload]} (Higher is better↑)"]

    # Get sorted variants and setup colors/hatches
    variants = sort_variants(data_workload['variant'].unique())
    colors = sns.color_palette("pastel", n_colors=len(variants))

    thread_counts = sorted(data_workload['n_clients'].unique())
    x_threads = range(len(thread_counts))
    width = 0.8 / len(variants)
    
    # Identify baseline, CVM, and GDPRuler variants
    baseline_variant = None
    cvm_variant = None
    gdpruler_variant = None
    for v in variants:
        if 'direct_bare_metal' in v and '-no_encr-' in v and '-tcp' in v:
            baseline_variant = v
        if 'direct_CVM' in v and '-no_encr-' in v and '-tcp' in v:
            cvm_variant = v
        if 'gdpr_CVM' in v and '-encr-' in v and '-unix' in v:
            gdpruler_variant = v
    
    # Statistics storage
    db_stats = {}

    for i, db in enumerate(dbs):
        ax = axes[i]
        df_db = data_workload[data_workload['db'] == db]
        
        print(f"\n{'-'*90}")
        print(f"{db.upper()} - Thread Scaling Analysis")
        print(f"{'-'*90}")
        
        db_stats[db] = {}

        for j, variant in enumerate(variants):
            df_var = df_db[df_db['variant'] == variant]
            if not df_var.empty:
                values, errors = prepare_plot_data(df_var, 'throughput', 'n_clients')
                offset = width * j - 0.4 + width / 2
                bar_positions = [xi + offset for xi in x_threads]
                ax.bar(bar_positions, values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
                if errors is not None:
                    ax.errorbar(bar_positions, values, yerr=errors,
                                fmt='none', ecolor='black', capsize=1, lw=0.5)
                
                # Store statistics
                db_stats[db][variant] = {
                    'thread_counts': thread_counts,
                    'throughputs': values.values if hasattr(values, 'values') else list(values),
                    'errors': errors.values if hasattr(errors, 'values') else list(errors) if errors is not None else None
                }
                
                # Print per-variant statistics
                baseline_type = variant.split('-')[0]
                label_text = variant_mapping.get(baseline_type, baseline_type)
                if 'no_encr' in variant and "direct_" not in variant:
                    label_text += " (w/o Encr)"
                
                print(f"\n{label_text}:")
                throughputs_list = db_stats[db][variant]['throughputs']
                if len(throughputs_list) > 0:
                    print(f"  Thread counts: {thread_counts}")
                    print(f"  Throughputs:   {[f'{t:.1f}' for t in throughputs_list]} kops/s")
                    if len(throughputs_list) >= 2:
                        speedup = throughputs_list[-1] / throughputs_list[0]
                        print(f"  Speedup (1→{thread_counts[-1]} threads): {speedup:.2f}×")
                        print(f"  Scaling efficiency: {(speedup / thread_counts[-1]) * 100:.1f}%")

        ax.set_xticks(x_threads)
        ax.set_xticklabels(thread_counts, fontsize=TICK_FONTSIZE_SEP)
        ax.tick_params(axis='x', length=0, pad=2)
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE_SEP, pad=1)
        ax.set_xlabel('Threads', fontsize=LABEL_FONTSIZE_SEP, labelpad=0)
        ax.set_ylabel('Throughput (kops/s)', fontsize=LABEL_FONTSIZE_SEP, labelpad=0)
        ax.set_title(titles[i], fontsize=TITLE_FONTSIZE_SEP, color="navy", pad=3)
        ax.grid(True, alpha=0.3, axis='y')
    
    # Print CVM overhead analysis
    if baseline_variant and cvm_variant:
        print(f"\n{'='*90}")
        print("CVM OVERHEAD ANALYSIS: CVM KVS vs Native Baseline")
        print(f"{'='*90}")
        
        for db in dbs:
            if baseline_variant in db_stats[db] and cvm_variant in db_stats[db]:
                baseline_data = db_stats[db][baseline_variant]
                cvm_data = db_stats[db][cvm_variant]
                
                print(f"\n{db.upper()}:")
                print(f"  Thread  |  Baseline  | CVM KVS    | CVM %    | Overhead")
                print(f"  --------|------------|------------|----------|----------")
                
                cvm_overheads = []
                for idx, thread in enumerate(thread_counts):
                    baseline_tput = baseline_data['throughputs'][idx]
                    cvm_tput = cvm_data['throughputs'][idx]
                    cvm_pct = (cvm_tput / baseline_tput * 100) if baseline_tput > 0 else 0
                    overhead = ((baseline_tput - cvm_tput) / baseline_tput * 100) if baseline_tput > 0 else 0
                    cvm_overheads.append(overhead)
                    
                    print(f"  {thread:>6}  | {baseline_tput:>8.1f}   | {cvm_tput:>8.1f}   | {cvm_pct:>6.1f}%  | {overhead:>7.1f}%")
                
                print(f"\n  CVM Overhead Summary:")
                print(f"    Average overhead: {sum(cvm_overheads)/len(cvm_overheads):.1f}%")
                print(f"    Min overhead: {min(cvm_overheads):.1f}%")
                print(f"    Max overhead: {max(cvm_overheads):.1f}%")
                print(f"    Overhead range: {min(cvm_overheads):.1f}%-{max(cvm_overheads):.1f}%")
    
    # Print comparative statistics (GDPRuler vs Baseline)
    if baseline_variant and gdpruler_variant:
        print(f"\n{'='*90}")
        print("COMPARATIVE ANALYSIS: GDPRuler vs Native Baseline")
        print(f"{'='*90}")
        
        for db in dbs:
            if baseline_variant in db_stats[db] and gdpruler_variant in db_stats[db]:
                baseline_data = db_stats[db][baseline_variant]
                gdpruler_data = db_stats[db][gdpruler_variant]
                
                print(f"\n{db.upper()}:")
                print(f"  Thread  |  Baseline  | GDPRuler   | Ratio    | Gap")
                print(f"  --------|------------|------------|----------|----------")
                
                for idx, thread in enumerate(thread_counts):
                    baseline_tput = baseline_data['throughputs'][idx]
                    gdpruler_tput = gdpruler_data['throughputs'][idx]
                    ratio = (gdpruler_tput / baseline_tput * 100) if baseline_tput > 0 else 0
                    gap = baseline_tput - gdpruler_tput
                    
                    print(f"  {thread:>6}  | {baseline_tput:>8.1f}   | {gdpruler_tput:>8.1f}   | {ratio:>6.1f}%  | {gap:>8.1f}")
                
                # Summary statistics
                ratios = [(gdpruler_data['throughputs'][i] / baseline_data['throughputs'][i] * 100) 
                         for i in range(len(thread_counts)) if baseline_data['throughputs'][i] > 0]
                gaps = [baseline_data['throughputs'][i] - gdpruler_data['throughputs'][i] 
                       for i in range(len(thread_counts))]
                
                print(f"\n  Summary:")
                print(f"    Average performance ratio: {sum(ratios)/len(ratios):.1f}%")
                print(f"    Min performance ratio: {min(ratios):.1f}%")
                print(f"    Max performance ratio: {max(ratios):.1f}%")
                print(f"    Average throughput gap: {sum(gaps)/len(gaps):.1f} kops/s")
                print(f"    Gap remains relatively constant: {'Yes' if max(gaps) - min(gaps) < 10 else 'No'}")
                
                # Check for crossover (Redis single-threaded behavior)
                crossover_thread = None
                for idx in range(len(thread_counts)):
                    if gdpruler_data['throughputs'][idx] >= baseline_data['throughputs'][idx]:
                        crossover_thread = thread_counts[idx]
                        break
                
                if crossover_thread:
                    print(f"    ⚠  Crossover at {crossover_thread} threads: GDPRuler >= Baseline")
                    print(f"       (Likely due to Redis single-threaded TCP bottleneck)")
    
    # Print GDPRuler overhead on top of CVM
    if cvm_variant and gdpruler_variant:
        print(f"\n{'='*90}")
        print("GDPR LAYER OVERHEAD: GDPRuler vs CVM KVS")
        print(f"{'='*90}")
        
        for db in dbs:
            if cvm_variant in db_stats[db] and gdpruler_variant in db_stats[db]:
                cvm_data = db_stats[db][cvm_variant]
                gdpruler_data = db_stats[db][gdpruler_variant]
                
                print(f"\n{db.upper()}:")
                print(f"  Thread  |  CVM KVS   | GDPRuler   | GDPR %   | Overhead")
                print(f"  --------|------------|------------|----------|----------")
                
                gdpr_overheads = []
                for idx, thread in enumerate(thread_counts):
                    cvm_tput = cvm_data['throughputs'][idx]
                    gdpruler_tput = gdpruler_data['throughputs'][idx]
                    gdpr_pct = (gdpruler_tput / cvm_tput * 100) if cvm_tput > 0 else 0
                    overhead = ((cvm_tput - gdpruler_tput) / cvm_tput * 100) if cvm_tput > 0 else 0
                    gdpr_overheads.append(overhead)
                    
                    print(f"  {thread:>6}  | {cvm_tput:>8.1f}   | {gdpruler_tput:>8.1f}   | {gdpr_pct:>6.1f}%  | {overhead:>7.1f}%")
                
                print(f"\n  GDPR Layer Overhead Summary:")
                print(f"    Average overhead: {sum(gdpr_overheads)/len(gdpr_overheads):.1f}%")
                print(f"    Min overhead: {min(gdpr_overheads):.1f}%")
                print(f"    Max overhead: {max(gdpr_overheads):.1f}%")
                print(f"    → This is the incremental cost of GDPR compliance on top of CVM")

    # Print appendix text suggestion
    print(f"\n{'='*90}")
    print("SUGGESTED APPENDIX TEXT")
    print(f"{'='*90}")
    
    workload_names = {
        'workloadb': 'Workload B (read-heavy, 95/5 R/W)',
        'workloadc': 'Workload C (read-only)',
        'workloadd': 'Workload D (read-latest, 95/5 R/I)',
        'workloadf': 'Workload F (read-modify-write)'
    }
    
    if workload in workload_names:
        print(f"\nFor {workload_names[workload]}:")
        
        for db in dbs:
            if baseline_variant in db_stats[db] and gdpruler_variant in db_stats[db]:
                baseline_data = db_stats[db][baseline_variant]
                gdpruler_data = db_stats[db][gdpruler_variant]
                
                min_threads = thread_counts[0]
                max_threads = thread_counts[-1]
                
                gdpr_min = gdpruler_data['throughputs'][0]
                gdpr_max = gdpruler_data['throughputs'][-1]
                base_min = baseline_data['throughputs'][0]
                base_max = baseline_data['throughputs'][-1]
                
                ratio_min = (gdpr_min / base_min * 100) if base_min > 0 else 0
                ratio_max = (gdpr_max / base_max * 100) if base_max > 0 else 0
                
                # Calculate ratios across all threads
                ratios = [(gdpruler_data['throughputs'][i] / baseline_data['throughputs'][i] * 100) 
                         for i in range(len(thread_counts)) if baseline_data['throughputs'][i] > 0]
                
                print(f"\n{db.upper()}:")
                print(f"  - At {min_threads} thread: GDPRuler achieves {gdpr_min:.1f} kops/s ({ratio_min:.1f}% of {base_min:.1f})")
                print(f"  - At {max_threads} threads: GDPRuler achieves {gdpr_max:.1f} kops/s ({ratio_max:.1f}% of {base_max:.1f})")
                print(f"  - Performance ratio range: {min(ratios):.1f}%-{max(ratios):.1f}%")
                
                # Add CVM overhead context if available
                if cvm_variant in db_stats[db]:
                    cvm_data = db_stats[db][cvm_variant]
                    cvm_overheads = [((baseline_data['throughputs'][i] - cvm_data['throughputs'][i]) / 
                                     baseline_data['throughputs'][i] * 100) 
                                    for i in range(len(thread_counts)) if baseline_data['throughputs'][i] > 0]
                    print(f"  - CVM overhead: {min(cvm_overheads):.1f}%-{max(cvm_overheads):.1f}%")
                    
                    gdpr_layer_overheads = [((cvm_data['throughputs'][i] - gdpruler_data['throughputs'][i]) / 
                                             cvm_data['throughputs'][i] * 100) 
                                           for i in range(len(thread_counts)) if cvm_data['throughputs'][i] > 0]
                    print(f"  - GDPR layer overhead (on top of CVM): {min(gdpr_layer_overheads):.1f}%-{max(gdpr_layer_overheads):.1f}%")

    print(f"{'='*90}\n")

    # Create legend
    handles, labels = [], []
    for j, variant in enumerate(variants):
        handles.append(plt.Rectangle((0,0),1,1, facecolor=colors[j], alpha=0.8, 
                                   hatch=hatches[j % len(hatches)], edgecolor='black'))
        baseline_type = variant.split('-')[0]
        label_text = variant_mapping.get(baseline_type, baseline_type)

        if 'no_encr' in variant and "direct_" not in variant:
            label_text += " (w/o Encr)"

        if include_tcp:
            if 'tcp' in variant:
                label_text += " (TCP)"
            else:
                label_text += " (UNIX)"

        labels.append(label_text)

    fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 1.12), 
              ncol=len(variants), fontsize=LEGEND_FONTSIZE_SEP, frameon=True,
              borderaxespad=0.5, columnspacing=0.45, labelspacing=0.35, borderpad=0.25, handletextpad=0.35, handlelength=1.2)

    plt.tight_layout()

    os.makedirs(output_dir, exist_ok=True)
    output_file = f"ycsb_{workload}_scalability" if not include_tcp else f"ycsb_{workload}_scalability_tcp"
    plt.savefig(os.path.join(output_dir, f'{output_file}.png'), dpi=300, pad_inches=0, bbox_inches='tight')
    plt.savefig(os.path.join(output_dir, f'{output_file}.pdf'), pad_inches=0, bbox_inches='tight')
    plt.close()

    print(f"✓ Plot saved: {output_file}")



def create_ycsb_performance_plot(data, output_dir, include_tcp = False):
    """Create paper-ready plots with 2 rows (DBs) x 3 columns layout"""
    
    # Filter data according to paper requirements
    data_filtered = filter_data_for_ycsb_performance_plot(data, include_tcp = include_tcp)

    # Define figure size (1/3 of double column for each subplot)
    fig, axes = plt.subplots(2, 3, figsize=(figwidth_full, 2.2))
    
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
        ['Redis - 1 Connection', 'Workload A (50/50 R/W)', 'Workload C (100/0 R/W)'],  # Top row (Redis)
        ['Rocksdb - 1 Connection', 'Workload A (50/50 R/W)', 'Workload C (100/0 R/W)']   # Bottom row (RocksDB)
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
                values, errors = prepare_plot_data(df_var, 'throughput', 'workload')
                offset = width * j - 0.4 + width / 2
                bar_positions = [xi + offset for xi in x_workloads]
                ax.bar(bar_positions, values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
                if errors is not None:
                    ax.errorbar(bar_positions, values, yerr=errors,
                                fmt='none', ecolor='black', capsize=1, lw=0.5)
        ax.set_xticks(x_workloads)
        ax.set_xticklabels([w[-1].upper() for w in workloads], fontsize=TICK_FONTSIZE)
        ax.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax.set_xlabel('YCSB Workload', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_ylabel('Throughput (kops)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_title(f"{title_prefixes[i][0]} {title_descriptions[i][0]} (Higher is better↑)", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
        ax.grid(True, alpha=0.3, axis='y')
        
        # Column 2: Throughput vs thread counts for workloadA (write-heavy)
        ax = axes[i, 1]
        df_subset = df_db[df_db['workload'] == 'workloada']
        
        for j, variant in enumerate(variants):
            df_var = df_subset[df_subset['variant'] == variant]
            if not df_var.empty:
                values, errors = prepare_plot_data(df_var, 'throughput', 'n_clients')
                offset = width * j - 0.4 + width / 2
                bar_positions = [xi + offset for xi in x_threads]
                ax.bar(bar_positions, values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
                if errors is not None:
                    ax.errorbar(bar_positions, values, yerr=errors,
                                fmt='none', ecolor='black', capsize=1, lw=0.5)
        
        ax.set_xticks(x_threads)
        ax.set_xticklabels(thread_counts, fontsize=TICK_FONTSIZE)
        ax.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax.set_xlabel('Threads', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_ylabel('Throughput (kops)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_title(f"{title_prefixes[i][1]} {title_descriptions[i][1]} (Higher is better↑)", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
        ax.grid(True, alpha=0.3, axis='y')
        
        # Column 3: Throughput vs thread counts for workloadC (read-heavy)
        ax = axes[i, 2]
        df_subset = df_db[df_db['workload'] == 'workloadc']
        
        for j, variant in enumerate(variants):
            df_var = df_subset[df_subset['variant'] == variant]
            if not df_var.empty:
                values, errors = prepare_plot_data(df_var, 'throughput', 'n_clients')
                offset = width * j - 0.4 + width / 2
                bar_positions = [xi + offset for xi in x_threads]
                ax.bar(bar_positions, values, width,
                      color=colors[j], alpha=0.8, hatch=hatches[j % len(hatches)],
                      edgecolor='black')
                if errors is not None:
                    ax.errorbar(bar_positions, values, yerr=errors,
                                fmt='none', ecolor='black', capsize=1, lw=0.5)
        
        ax.set_xticks(x_threads)
        ax.set_xticklabels(thread_counts, fontsize=TICK_FONTSIZE)
        ax.tick_params(axis='x', length=0, pad=2)  # Remove x-axis tick bars
        ax.tick_params(axis='y', labelsize=TICK_FONTSIZE, pad=2)
        ax.set_xlabel('Threads', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_ylabel('Throughput (kops)', fontsize=LABEL_FONTSIZE, labelpad=2)
        ax.set_title(f"{title_prefixes[i][2]} {title_descriptions[i][2]} (Higher is better↑)", fontsize=TITLE_FONTSIZE, color="navy", pad=3)
        ax.grid(True, alpha=0.3, axis='y')
        
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
        if 'no_encr' in variant and "direct_" not in variant:
            label_text += " (w/o Encr)"
        # else:
        #     label_text += " (w/ Encr)"

        if include_tcp:
          if 'tcp' in variant:
            label_text += " (TCP)"
          else:
            label_text += " (UNIX)"

        labels.append(label_text)
    
    # Position legend at the top
    fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 1.1), 
              ncol=len(variants), fontsize=LEGEND_FONTSIZE, frameon=True)
    
    # Adjust layout and save
    plt.tight_layout()
    
    os.makedirs(output_dir, exist_ok=True)
    output_file = "ycsb_performance_plot" if not include_tcp else "ycsb_performance_plot_tcp"
    plt.savefig(os.path.join(output_dir, f'{output_file}.png'),
                bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, f'{output_file}.pdf'),
                bbox_inches='tight')
    plt.close(fig)
    
    # Print statistics
    print_ycsb_statistics(data_filtered, variants)
    
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
    # create_ycsb_performance_plot(all_data, args.output_dir)
    # create_ycsb_performance_plot(all_data, args.output_dir, include_tcp = True)
    
    create_single_thread_comparison_plot(all_data, args.output_dir)
    create_workload_scalability_plot(all_data, args.output_dir, "workloada")
    create_workload_scalability_plot(all_data, args.output_dir, "workloadb")
    create_workload_scalability_plot(all_data, args.output_dir, "workloadc")
    create_workload_scalability_plot(all_data, args.output_dir, "workloadd")
    create_workload_scalability_plot(all_data, args.output_dir, "workloadf")
    
if __name__ == "__main__":
    main()
