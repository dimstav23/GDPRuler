import pandas as pd 
import matplotlib as mpl
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os
import argparse
from itertools import product

# Set matplotlib backend and styling
mpl.use("Agg")
mpl.rcParams["text.latex.preamble"] = r"\usepackage{amsmath}"
mpl.rcParams["pdf.fonttype"] = 42
mpl.rcParams["ps.fonttype"] = 42
mpl.rcParams["font.family"] = "libertine"

sns.set_style("whitegrid")
sns.set_style("ticks", {"xtick.major.size": 8, "ytick.major.size": 8})
sns.set_context("paper", rc={"font.size": 5, "axes.titlesize": 5, "axes.labelsize": 8})

# Figure dimensions
figwidth_half = 3.3
figwidth_full = 7

# Font sizes
FONTSIZE = 7
TITLE_FONTSIZE = FONTSIZE
LABEL_FONTSIZE = FONTSIZE
TICK_FONTSIZE = FONTSIZE - 1
LEGEND_FONTSIZE = FONTSIZE - 1

FONTSIZE = 7
TITLE_FONTSIZE = FONTSIZE
LABEL_FONTSIZE = FONTSIZE
TICK_FONTSIZE = FONTSIZE - 1
LEGEND_FONTSIZE = FONTSIZE - 0.5
ANNOTATION_FONTSIZE = FONTSIZE / 2

# Patterns for bar plots
hatches = ['', '///', '\\\\\\', 'xxx', '...', '+++', '|||', '---', 'ooo', '***']

def load_gdpr_data(input_file):
    """Load and preprocess GDPR benchmark data"""
    df = pd.read_csv(input_file)
    print(df.columns.tolist)
    # Create categorical labels for better plotting
    df['encryption_label'] = df['use_encryption'].map({0: 'No Encryption', 1: 'With Encryption'})
    df['compression_label'] = df['compression_level'].map({0: 'No Compression', 5: 'Medium (5)', 9: 'High (9)'})
    df['entry_size_label'] = df['entry_size_bytes'].map({1024: '1KB', 2048: '2KB', 4096: '4KB'})

    return df

def create_encryption_batch_analysis_plot(df, output_dir):
    """Create specific 2-subplot analysis: throughput and write amplification vs batch size"""
    
    # Filter data: compression=0, encryption=1
    filtered_df = df[(df['compression_level'] == 0) & (df['use_encryption'] == 1)]
    
    if filtered_df.empty:
        print("No data found for compression=0 and encryption=1")
        return
    
    print("\n" + "="*80)
    print("BATCH SIZE ANALYSIS STATISTICS (Encryption=ON, Compression=OFF)")
    print("="*80)
    
    # Create figure with 2 subplots side by side
    fig, axes = plt.subplots(1, 2, figsize=(figwidth_half, 1.4))
    
    # Define the specific variants we want to show
    variants = [
        {'consumers': 4, 'entry_size_bytes': 256, 'label': 'W:4,E:256', 'color': '#9467bd', 'marker': 'v'},
        {'consumers': 8, 'entry_size_bytes': 256, 'label': 'W:8,E:256', 'color': '#8c564b', 'marker': 'p'},
        {'consumers': 4, 'entry_size_bytes': 1024, 'label': 'W:4,E:1K', 'color': '#1f77b4', 'marker': 'o'},
        {'consumers': 8, 'entry_size_bytes': 1024, 'label': 'W:8,E:1K', 'color': '#2ca02c', 'marker': '^'},
        {'consumers': 4, 'entry_size_bytes': 4096, 'label': 'W:4,E:4K', 'color': '#ff7f0e', 'marker': 's'},        
        {'consumers': 8, 'entry_size_bytes': 4096, 'label': 'W:8,E:4K', 'color': '#d62728', 'marker': 'D'}
    ]
    
    # Get batch sizes from data and prepare x-axis mapping
    batch_sizes = sorted(filtered_df['batch_size'].unique())
    x_positions = list(range(1, len(batch_sizes) + 1))
    batch_labels = [str(bs) for bs in batch_sizes]
    
    print(f"\nBatch sizes analyzed: {batch_sizes}")
    print(f"Number of variants: {len(variants)}")
    
    # Statistics storage
    throughput_stats = {}
    wa_stats = {}
    
    # Left subplot: Throughput vs Batch Size
    ax_left = axes[0]
    
    print("\n" + "-"*80)
    print("THROUGHPUT STATISTICS (K entries/sec)")
    print("-"*80)
    
    for variant in variants:
        # Filter data for this specific variant
        variant_df = filtered_df[
            (filtered_df['consumers'] == variant['consumers']) & 
            (filtered_df['entry_size_bytes'] == variant['entry_size_bytes'])
        ]
        
        if not variant_df.empty:
            print(f"\n{variant['label']}:")
            
            # Group by batch size and get statistics
            throughput_data = variant_df.groupby('batch_size')['entries_per_sec'].agg(['mean', 'std', 'min', 'max', 'count'])
            
            # Prepare data for plotting
            x_vals = []
            y_vals = []
            for i, bs in enumerate(batch_sizes):
                if bs in throughput_data.index:
                    x_vals.append(x_positions[i])
                    y_vals.append(throughput_data.loc[bs, 'mean'] / 1000)
                    
                    # Print statistics
                    print(f"  Batch {bs:>5}: "
                          f"Mean={throughput_data.loc[bs, 'mean']/1000:>7.2f} K/s, "
                          f"Std={throughput_data.loc[bs, 'std']/1000:>6.2f}, "
                          f"Min={throughput_data.loc[bs, 'min']/1000:>7.2f}, "
                          f"Max={throughput_data.loc[bs, 'max']/1000:>7.2f}, "
                          f"N={int(throughput_data.loc[bs, 'count'])}")
            
            # Store stats
            throughput_stats[variant['label']] = throughput_data
            
            # Calculate improvement from smallest to largest batch
            if len(y_vals) >= 2:
                improvement = (y_vals[-1] / y_vals[0] - 1) * 100
                print(f"  → Throughput improvement (min→max batch): {improvement:+.1f}%")
            
            # Plot line with markers
            ax_left.plot(x_vals, y_vals, 
                        marker=variant['marker'], 
                        color=variant['color'],
                        linewidth=0.8,
                        markersize=1.5,
                        label=variant['label'])
    
    ax_left.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE, labelpad=1)
    ax_left.set_ylabel('Throughput (K entries/s)', fontsize=LABEL_FONTSIZE, labelpad=0)
    ax_left.set_title('(a) Throughput\n(Higher is better ↑)', color="navy", fontsize=TITLE_FONTSIZE, pad=3)
    ax_left.set_xticks(x_positions)
    ax_left.tick_params(axis='x', length=2)
    ax_left.set_xticklabels(batch_labels)
    ax_left.tick_params(axis='both', labelsize=TICK_FONTSIZE, pad=1)
    ax_left.grid(True, alpha=0.3, axis='y')
    ax_left.tick_params(axis='y', length=2)
    
    # Right subplot: Write Amplification vs Batch Size
    ax_right = axes[1]
    
    print("\n" + "-"*80)
    print("WRITE AMPLIFICATION STATISTICS (%)")
    print("-"*80)
    
    for variant in variants:
        # Filter data for this specific variant
        variant_df = filtered_df[
            (filtered_df['consumers'] == variant['consumers']) & 
            (filtered_df['entry_size_bytes'] == variant['entry_size_bytes'])
        ]
        
        if not variant_df.empty:
            print(f"\n{variant['label']}:")
            
            # Group by batch size and get statistics
            wa_data = variant_df.groupby('batch_size')['write_amplification'].agg(['mean', 'std', 'min', 'max', 'count'])
            
            # Prepare data for plotting
            x_vals = []
            y_vals = []
            for i, bs in enumerate(batch_sizes):
                if bs in wa_data.index:
                    x_vals.append(x_positions[i])
                    # Convert to percentage
                    y_vals.append((wa_data.loc[bs, 'mean'] - 1) * 100)
                    
                    # Print statistics
                    mean_pct = (wa_data.loc[bs, 'mean'] - 1) * 100
                    std_pct = wa_data.loc[bs, 'std'] * 100
                    min_pct = (wa_data.loc[bs, 'min'] - 1) * 100
                    max_pct = (wa_data.loc[bs, 'max'] - 1) * 100
                    
                    print(f"  Batch {bs:>5}: "
                          f"Mean={mean_pct:>6.2f}%, "
                          f"Std={std_pct:>5.2f}, "
                          f"Min={min_pct:>6.2f}%, "
                          f"Max={max_pct:>6.2f}%, "
                          f"N={int(wa_data.loc[bs, 'count'])}")
            
            # Store stats
            wa_stats[variant['label']] = wa_data
            
            # Calculate reduction from smallest to largest batch
            if len(y_vals) >= 2:
                reduction = y_vals[0] - y_vals[-1]
                print(f"  → Write amplification reduction (min→max batch): {reduction:.2f} percentage points")
            
            # Plot line with markers
            ax_right.plot(x_vals, y_vals, 
                         marker=variant['marker'], 
                         color=variant['color'],
                         linewidth=0.8,
                         markersize=1.5,
                         label=variant['label'])
    
    ax_right.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE, labelpad=1)
    ax_right.set_ylabel('Write Amplification (%)', fontsize=LABEL_FONTSIZE, labelpad=0)
    ax_right.set_title('(b) Write Amplification\n(Lower is better ↓)', color="navy", fontsize=TITLE_FONTSIZE, pad=3)
    ax_right.set_xticks(x_positions)
    ax_right.set_xticklabels(batch_labels)
    ax_right.tick_params(axis='x', length=2)
    ax_right.tick_params(axis='y', length=2)
    ax_right.tick_params(axis='both', labelsize=TICK_FONTSIZE, pad=1)
    ax_right.grid(True, alpha=0.3, axis='y')
    
    # Print summary statistics
    print("\n" + "="*80)
    print("SUMMARY STATISTICS")
    print("="*80)
    
    print("\nThroughput Summary:")
    all_throughputs = []
    for label, stats in throughput_stats.items():
        all_vals = stats['mean'].values / 1000
        all_throughputs.extend(all_vals)
        print(f"  {label}: Range {all_vals.min():.2f}-{all_vals.max():.2f} K/s, "
              f"Avg={all_vals.mean():.2f} K/s")
    
    print(f"\n  Overall throughput range: {min(all_throughputs):.2f}-{max(all_throughputs):.2f} K entries/sec")
    print(f"  Overall average: {np.mean(all_throughputs):.2f} K entries/sec")
    
    print("\nWrite Amplification Summary:")
    all_wa = []
    for label, stats in wa_stats.items():
        all_vals = (stats['mean'].values - 1) * 100
        all_wa.extend(all_vals)
        print(f"  {label}: Range {all_vals.min():.2f}-{all_vals.max():.2f}%, "
              f"Avg={all_vals.mean():.2f}%")
    
    print(f"\n  Overall WA range: {min(all_wa):.2f}-{max(all_wa):.2f}%")
    print(f"  Overall average: {np.mean(all_wa):.2f}%")
    
    # Add single legend to the figure
    handles, labels = ax_left.get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 1.1), 
               ncol=6, fontsize=LEGEND_FONTSIZE,
               borderaxespad=0.4, columnspacing=0.3, labelspacing=0.25, borderpad=0.15, handletextpad=0.3, handlelength=0.8)
    
    # Adjust layout to make room for legend
    plt.tight_layout()
    
    # Save the plot
    plt.savefig(os.path.join(output_dir, 'batch_analysis.png'), 
                bbox_inches='tight', dpi=300, pad_inches=0)
    plt.savefig(os.path.join(output_dir, 'batch_analysis.pdf'), 
                bbox_inches='tight', dpi=300, pad_inches=0)
    plt.close()
    
    print("\n" + "="*80)
    print(f"✓ Plot saved to {output_dir}/batch_analysis.png|pdf")
    print("="*80 + "\n")

        
def create_encryption_effect_plots(df, output_dir):
    """Plot 1-2: Effect of Encryption on Performance across all configurations"""
    fig, axes = plt.subplots(2, 2, figsize=(figwidth_full, 6))
    
    # Plot 1: Throughput by encryption across batch sizes
    ax = axes[0, 0]
    batch_sizes = sorted(df['batch_size'].unique())
    x_positions = np.arange(len(batch_sizes))
    width = 0.35
    
    no_enc = df[df['use_encryption'] == 0].groupby('batch_size')['entries_per_sec'].mean()
    with_enc = df[df['use_encryption'] == 1].groupby('batch_size')['entries_per_sec'].mean()
    
    ax.bar(x_positions - width/2, no_enc.values / 1000, width, 
           label='No Encryption', alpha=0.8, hatch=hatches[0], edgecolor='black')
    ax.bar(x_positions + width/2, with_enc.values / 1000, width,
           label='With Encryption', alpha=0.8, hatch=hatches[1], edgecolor='black')
    
    ax.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(a) Encryption Impact vs Batch Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions)
    ax.set_xticklabels([str(bs) for bs in batch_sizes], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    # Plot 2: Throughput by encryption across entry sizes
    ax = axes[0, 1]
    entry_sizes = sorted(df['entry_size_bytes'].unique())
    x_positions = np.arange(len(entry_sizes))
    
    no_enc = df[df['use_encryption'] == 0].groupby('entry_size_bytes')['entries_per_sec'].mean()
    with_enc = df[df['use_encryption'] == 1].groupby('entry_size_bytes')['entries_per_sec'].mean()
    
    ax.bar(x_positions - width/2, no_enc.values / 1000, width, 
           label='No Encryption', alpha=0.8, hatch=hatches[0], edgecolor='black')
    ax.bar(x_positions + width/2, with_enc.values / 1000, width,
           label='With Encryption', alpha=0.8, hatch=hatches[1], edgecolor='black')
    
    ax.set_xlabel('Entry Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(b) Encryption Impact vs Entry Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions)
    ax.set_xticklabels([f'{es//1024}KB' for es in entry_sizes], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    # Plot 3: Write amplification by encryption across compression levels
    ax = axes[1, 0]
    comp_levels = sorted(df['compression_level'].unique())
    x_positions = np.arange(len(comp_levels))
    
    no_enc = df[df['use_encryption'] == 0].groupby('compression_level')['write_amplification'].mean()
    with_enc = df[df['use_encryption'] == 1].groupby('compression_level')['write_amplification'].mean()
    
    ax.bar(x_positions - width/2, no_enc.values, width, 
           label='No Encryption', alpha=0.8, hatch=hatches[0], edgecolor='black')
    ax.bar(x_positions + width/2, with_enc.values, width,
           label='With Encryption', alpha=0.8, hatch=hatches[1], edgecolor='black')
    
    ax.set_xlabel('Compression Level', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Write Amplification', fontsize=LABEL_FONTSIZE)
    ax.set_title('(c) Encryption Impact on Write Amplification', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions)
    ax.set_xticklabels([str(cl) for cl in comp_levels], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    # Plot 4: Throughput by encryption across writer threads
    ax = axes[1, 1]
    consumers_list = sorted(df['consumers'].unique())
    x_positions = np.arange(len(consumers_list))
    
    no_enc = df[df['use_encryption'] == 0].groupby('consumers')['entries_per_sec'].mean()
    with_enc = df[df['use_encryption'] == 1].groupby('consumers')['entries_per_sec'].mean()
    
    ax.bar(x_positions - width/2, no_enc.values / 1000, width, 
           label='No Encryption', alpha=0.8, hatch=hatches[0], edgecolor='black')
    ax.bar(x_positions + width/2, with_enc.values / 1000, width,
           label='With Encryption', alpha=0.8, hatch=hatches[1], edgecolor='black')
    
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(d) Encryption Impact vs Writer Threads', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions)
    ax.set_xticklabels([str(c) for c in consumers_list], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'encryption_effect.png'), bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, 'encryption_effect.pdf'), bbox_inches='tight')
    plt.close()

def create_compression_effect_plots(df, output_dir):
    """Plot 3-4: Effect of Compression on Performance across all configurations"""
    fig, axes = plt.subplots(2, 2, figsize=(figwidth_full, 6))
    
    compression_levels = sorted(df['compression_level'].unique())
    colors = sns.color_palette("viridis", len(compression_levels))
    
    # Plot 1: Throughput by compression across batch sizes
    ax = axes[0, 0]
    batch_sizes = sorted(df['batch_size'].unique())
    x_positions = np.arange(len(batch_sizes))
    width = 0.25
    
    for i, comp_level in enumerate(compression_levels):
        data = df[df['compression_level'] == comp_level].groupby('batch_size')['entries_per_sec'].mean()
        throughput = [data[bs] / 1000 if bs in data.index else 0 for bs in batch_sizes]
        
        ax.bar(x_positions + i*width, throughput, width,
               label=f'Compression {comp_level}', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(a) Compression vs Batch Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels([str(bs) for bs in batch_sizes], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    # Plot 2: Throughput by compression across entry sizes
    ax = axes[0, 1]
    entry_sizes = sorted(df['entry_size_bytes'].unique())
    x_positions = np.arange(len(entry_sizes))
    
    for i, comp_level in enumerate(compression_levels):
        data = df[df['compression_level'] == comp_level].groupby('entry_size_bytes')['entries_per_sec'].mean()
        throughput = [data[es] / 1000 if es in data.index else 0 for es in entry_sizes]
        
        ax.bar(x_positions + i*width, throughput, width,
               label=f'Compression {comp_level}', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Entry Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(b) Compression vs Entry Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels([f'{es//1024}KB' for es in entry_sizes], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    # Plot 3: Write amplification by compression across encryption settings
    ax = axes[1, 0]
    encryption_settings = [0, 1]
    x_positions = np.arange(len(encryption_settings))
    
    for i, comp_level in enumerate(compression_levels):
        data = df[df['compression_level'] == comp_level].groupby('use_encryption')['write_amplification'].mean()
        wa = [data[enc] if enc in data.index else 1.0 for enc in encryption_settings]
        
        ax.bar(x_positions + i*width, wa, width,
               label=f'Compression {comp_level}', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Encryption Setting', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Write Amplification', fontsize=LABEL_FONTSIZE)
    ax.set_title('(c) Compression vs Encryption', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels(['Off', 'On'], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    # Plot 4: Write amplification by compression across writer threads
    ax = axes[1, 1]
    consumers_list = sorted(df['consumers'].unique())
    x_positions = np.arange(len(consumers_list))
    
    for i, comp_level in enumerate(compression_levels):
        data = df[df['compression_level'] == comp_level].groupby('consumers')['write_amplification'].mean()
        wa = [data[c] if c in data.index else 1.0 for c in consumers_list]
        
        ax.bar(x_positions + i*width, wa, width,
               label=f'Compression {comp_level}', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Write Amplification', fontsize=LABEL_FONTSIZE)
    ax.set_title('(d) Compression vs Writer Threads', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels([str(c) for c in consumers_list], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'compression_effect.png'), bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, 'compression_effect.pdf'), bbox_inches='tight')
    plt.close()

def create_entry_size_effect_plots(df, output_dir):
    """Plot 5-6: Effect of Entry Size on Performance across all configurations"""
    fig, axes = plt.subplots(2, 2, figsize=(figwidth_full, 6))
    
    entry_sizes = sorted(df['entry_size_bytes'].unique())
    colors = sns.color_palette("Set2", len(entry_sizes))
    
    # Plot 1: Entry throughput by entry size across batch sizes
    ax = axes[0, 0]
    batch_sizes = sorted(df['batch_size'].unique())
    x_positions = np.arange(len(batch_sizes))
    width = 0.25
    
    for i, entry_size in enumerate(entry_sizes):
        data = df[df['entry_size_bytes'] == entry_size].groupby('batch_size')['entries_per_sec'].mean()
        throughput = [data[bs] / 1000 if bs in data.index else 0 for bs in batch_sizes]
        
        ax.bar(x_positions + i*width, throughput, width,
               label=f'{entry_size//1024}KB', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(a) Entry Size vs Batch Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels([str(bs) for bs in batch_sizes], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE, title='Entry Size')
    
    # Plot 2: Data throughput by entry size across compression
    ax = axes[0, 1]
    comp_levels = sorted(df['compression_level'].unique())
    x_positions = np.arange(len(comp_levels))
    
    for i, entry_size in enumerate(entry_sizes):
        data = df[df['entry_size_bytes'] == entry_size].groupby('compression_level')['logical_throughput_gib_sec'].mean()
        throughput = [data[cl] if cl in data.index else 0 for cl in comp_levels]
        
        ax.bar(x_positions + i*width, throughput, width,
               label=f'{entry_size//1024}KB', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Compression Level', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Data Throughput (GiB/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(b) Entry Size vs Compression', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels([str(cl) for cl in comp_levels], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE, title='Entry Size')
    
    # Plot 3: Throughput by entry size across encryption
    ax = axes[1, 0]
    encryption_settings = [0, 1]
    x_positions = np.arange(len(encryption_settings))
    
    for i, entry_size in enumerate(entry_sizes):
        data = df[df['entry_size_bytes'] == entry_size].groupby('use_encryption')['entries_per_sec'].mean()
        throughput = [data[enc] / 1000 if enc in data.index else 0 for enc in encryption_settings]
        
        ax.bar(x_positions + i*width, throughput, width,
               label=f'{entry_size//1024}KB', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Encryption', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(c) Entry Size vs Encryption', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels(['Off', 'On'], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE, title='Entry Size')
    
    # Plot 4: Throughput by entry size across writer threads
    ax = axes[1, 1]
    consumers_list = sorted(df['consumers'].unique())
    x_positions = np.arange(len(consumers_list))
    
    for i, entry_size in enumerate(entry_sizes):
        data = df[df['entry_size_bytes'] == entry_size].groupby('consumers')['entries_per_sec'].mean()
        throughput = [data[c] / 1000 if c in data.index else 0 for c in consumers_list]
        
        ax.bar(x_positions + i*width, throughput, width,
               label=f'{entry_size//1024}KB', alpha=0.8, 
               color=colors[i], hatch=hatches[i], edgecolor='black')
    
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(d) Entry Size vs Writer Threads', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xticks(x_positions + width)
    ax.set_xticklabels([str(c) for c in consumers_list], fontsize=TICK_FONTSIZE)
    ax.legend(fontsize=LEGEND_FONTSIZE, title='Entry Size')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'entry_size_effect.png'), bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, 'entry_size_effect.pdf'), bbox_inches='tight')
    plt.close()

def create_writer_threads_effect_plots(df, output_dir):
    """Plot 7-8: Effect of Writer Threads on Performance across all configurations"""
    fig, axes = plt.subplots(2, 2, figsize=(figwidth_full, 6))
    
    consumers_list = sorted(df['consumers'].unique())
    colors = sns.color_palette("Set1", len(consumers_list))
    
    # Plot 1: Throughput vs Writer Threads across batch sizes (line plot)
    ax = axes[0, 0]
    batch_sizes = sorted(df['batch_size'].unique())
    batch_colors = sns.color_palette("plasma", len(batch_sizes))
    
    for i, batch_size in enumerate(batch_sizes):
        data = df[df['batch_size'] == batch_size].groupby('consumers')['entries_per_sec'].mean()
        throughput = [data[c] / 1000 if c in data.index else 0 for c in consumers_list]
        
        ax.plot(consumers_list, throughput, marker='o', linewidth=2,
               label=f'Batch {batch_size}', color=batch_colors[i])
    
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(a) Writer Threads vs Batch Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE, title='Batch Size')
    ax.grid(True, alpha=0.3)
    
    # Plot 2: Throughput vs Writer Threads across encryption/compression
    ax = axes[0, 1]
    
    # Create combinations of encryption and compression
    enc_comp_combinations = [(0, 0), (0, 9), (1, 0), (1, 9)]  # Key combinations
    comb_colors = sns.color_palette("tab10", len(enc_comp_combinations))
    comb_labels = ['No Enc, No Comp', 'No Enc, High Comp', 'Enc, No Comp', 'Enc, High Comp']
    
    for i, (enc, comp) in enumerate(enc_comp_combinations):
        subset = df[(df['use_encryption'] == enc) & (df['compression_level'] == comp)]
        if not subset.empty:
            data = subset.groupby('consumers')['entries_per_sec'].mean()
            throughput = [data[c] / 1000 if c in data.index else 0 for c in consumers_list]
            
            ax.plot(consumers_list, throughput, marker='s', linewidth=2,
                   label=comb_labels[i], color=comb_colors[i])
    
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(b) Writer Threads vs Enc/Comp', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE - 1)
    ax.grid(True, alpha=0.3)
    
    # Plot 3: Scaling efficiency across entry sizes
    ax = axes[1, 0]
    entry_sizes = sorted(df['entry_size_bytes'].unique())
    entry_colors = sns.color_palette("Set2", len(entry_sizes))
    
    for i, entry_size in enumerate(entry_sizes):
        data = df[df['entry_size_bytes'] == entry_size].groupby('consumers')['entries_per_sec'].mean()
        if consumers_list[0] in data.index and data[consumers_list[0]] > 0:
            baseline = data[consumers_list[0]]
            normalized = [data[c] / baseline if c in data.index else 0 for c in consumers_list]
            
            ax.plot(consumers_list, normalized, marker='^', linewidth=2,
                   label=f'{entry_size//1024}KB', color=entry_colors[i])
    
    # Add ideal scaling line
    ax.plot(consumers_list, [c/consumers_list[0] for c in consumers_list], 
           'k--', alpha=0.5, label='Ideal Scaling')
    
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Normalized Throughput', fontsize=LABEL_FONTSIZE)
    ax.set_title('(c) Scaling Efficiency by Entry Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    ax.grid(True, alpha=0.3)
    
    # Plot 4: Latency vs Writer Threads across configurations
    ax = axes[1, 1]
    
    for i, (enc, comp) in enumerate(enc_comp_combinations):
        subset = df[(df['use_encryption'] == enc) & (df['compression_level'] == comp)]
        if not subset.empty:
            data = subset.groupby('consumers')['avg_latency_ms'].mean()
            latency = [data[c] if c in data.index else 0 for c in consumers_list]
            
            ax.plot(consumers_list, latency, marker='D', linewidth=2,
                   label=comb_labels[i], color=comb_colors[i])
    
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Average Latency (ms)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(d) Latency vs Writer Threads', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE - 1)
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'writer_threads_effect.png'), bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, 'writer_threads_effect.pdf'), bbox_inches='tight')
    plt.close()

def create_batch_size_effect_plots(df, output_dir):
    """Plot 9-10: Effect of Batch Size on Performance across all configurations"""
    fig, axes = plt.subplots(2, 2, figsize=(figwidth_full, 6))
    
    batch_sizes = sorted(df['batch_size'].unique())
    
    # Plot 1: Throughput vs Batch Size across encryption/compression (line plot)
    ax = axes[0, 0]
    
    # Create combinations of encryption and compression
    enc_comp_combinations = [(0, 0), (0, 5), (0, 9), (1, 0), (1, 5), (1, 9)]
    comb_colors = sns.color_palette("tab10", len(enc_comp_combinations))
    comb_labels = ['No Enc, No Comp', 'No Enc, Med Comp', 'No Enc, High Comp',
                   'Enc, No Comp', 'Enc, Med Comp', 'Enc, High Comp']
    
    for i, (enc, comp) in enumerate(enc_comp_combinations):
        subset = df[(df['use_encryption'] == enc) & (df['compression_level'] == comp)]
        if not subset.empty:
            data = subset.groupby('batch_size')['entries_per_sec'].mean()
            throughput = [data[bs] / 1000 if bs in data.index else 0 for bs in batch_sizes]
            
            ax.plot(batch_sizes, throughput, marker='o', linewidth=2,
                   label=comb_labels[i], color=comb_colors[i])
    
    ax.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(a) Batch Size vs Enc/Comp Settings', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE - 2, ncol=2)
    ax.grid(True, alpha=0.3)
    
    # Plot 2: Throughput vs Batch Size across entry sizes
    ax = axes[0, 1]
    entry_sizes = sorted(df['entry_size_bytes'].unique())
    entry_colors = sns.color_palette("viridis", len(entry_sizes))
    
    for i, entry_size in enumerate(entry_sizes):
        data = df[df['entry_size_bytes'] == entry_size].groupby('batch_size')['entries_per_sec'].mean()
        throughput = [data[bs] / 1000 if bs in data.index else 0 for bs in batch_sizes]
        
        ax.plot(batch_sizes, throughput, marker='s', linewidth=2,
               label=f'{entry_size//1024}KB', color=entry_colors[i])
    
    ax.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Throughput (K entries/sec)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(b) Batch Size vs Entry Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE, title='Entry Size')
    ax.grid(True, alpha=0.3)
    
    # Plot 3: Latency vs Batch Size across writer threads
    ax = axes[1, 0]
    consumers_list = sorted(df['consumers'].unique())
    thread_colors = sns.color_palette("Set1", len(consumers_list))
    
    for i, consumers in enumerate(consumers_list):
        data = df[df['consumers'] == consumers].groupby('batch_size')['avg_latency_ms'].mean()
        latency = [data[bs] if bs in data.index else 0 for bs in batch_sizes]
        
        ax.plot(batch_sizes, latency, marker='^', linewidth=2,
               label=f'{consumers} Writers', color=thread_colors[i])
    
    ax.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Average Latency (ms)', fontsize=LABEL_FONTSIZE)
    ax.set_title('(c) Batch Size vs Latency', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    ax.grid(True, alpha=0.3)
    
    # Plot 4: Write amplification vs Batch Size across all combinations
    ax = axes[1, 1]
    
    # Show selected combinations for clarity
    selected_combinations = [(0, 0), (1, 0), (0, 9), (1, 9)]
    selected_labels = ['No Enc, No Comp', 'Enc, No Comp', 'No Enc, High Comp', 'Enc, High Comp']
    selected_colors = ['blue', 'red', 'green', 'purple']
    
    for i, (enc, comp) in enumerate(selected_combinations):
        subset = df[(df['use_encryption'] == enc) & (df['compression_level'] == comp)]
        if not subset.empty:
            data = subset.groupby('batch_size')['write_amplification'].mean()
            wa = [data[bs] if bs in data.index else 1.0 for bs in batch_sizes]
            
            ax.plot(batch_sizes, wa, marker='D', linewidth=2,
                   label=selected_labels[i], color=selected_colors[i])
    
    ax.set_xlabel('Batch Size', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Write Amplification', fontsize=LABEL_FONTSIZE)
    ax.set_title('(d) Batch Size vs Write Amplification', fontsize=TITLE_FONTSIZE, pad=5)
    ax.legend(fontsize=LEGEND_FONTSIZE)
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'batch_size_effect.png'), bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, 'batch_size_effect.pdf'), bbox_inches='tight')
    plt.close()

def create_heatmaps(df, output_dir):
    """Plot 11-12: Performance heatmaps"""
    fig, axes = plt.subplots(2, 2, figsize=(figwidth_full, 6))
    
    # Heatmap 1: Throughput by batch size vs compression (all entry sizes)
    ax = axes[0, 0]
    pivot_data = df.groupby(['batch_size', 'compression_level'])['entries_per_sec'].mean().unstack()
    pivot_data = pivot_data / 1000  # Convert to K entries/sec
    
    sns.heatmap(pivot_data, annot=True, fmt='.0f', cmap='YlOrRd', 
                ax=ax, cbar_kws={'label': 'K entries/sec'})
    ax.set_title('(a) Throughput: Batch Size vs Compression', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xlabel('Compression Level', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Batch Size', fontsize=LABEL_FONTSIZE)
    
    # Heatmap 2: Write amplification by entry size vs encryption
    ax = axes[0, 1]
    pivot_data = df.groupby(['entry_size_bytes', 'use_encryption'])['write_amplification'].mean().unstack()
    
    sns.heatmap(pivot_data, annot=True, fmt='.3f', cmap='RdYlBu_r', 
                ax=ax, cbar_kws={'label': 'Write Amplification'})
    ax.set_title('(b) Write Amplification: Size vs Encryption', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xlabel('Encryption (0=Off, 1=On)', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Entry Size (bytes)', fontsize=LABEL_FONTSIZE)
    
    # Heatmap 3: Throughput by writer threads vs entry size
    ax = axes[1, 0]
    pivot_data = df.groupby(['consumers', 'entry_size_bytes'])['entries_per_sec'].mean().unstack()
    pivot_data = pivot_data / 1000  # Convert to K entries/sec
    
    sns.heatmap(pivot_data, annot=True, fmt='.0f', cmap='plasma', 
                ax=ax, cbar_kws={'label': 'K entries/sec'})
    ax.set_title('(c) Throughput: Writers vs Entry Size', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xlabel('Entry Size (bytes)', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    
    # Heatmap 4: Latency by batch size vs writer threads
    ax = axes[1, 1]
    pivot_data = df.groupby(['batch_size', 'consumers'])['avg_latency_ms'].mean().unstack()
    
    sns.heatmap(pivot_data, annot=True, fmt='.2f', cmap='viridis_r', 
                ax=ax, cbar_kws={'label': 'Latency (ms)'})
    ax.set_title('(d) Latency: Batch Size vs Writers', fontsize=TITLE_FONTSIZE, pad=5)
    ax.set_xlabel('Writer Threads', fontsize=LABEL_FONTSIZE)
    ax.set_ylabel('Batch Size', fontsize=LABEL_FONTSIZE)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'heatmaps.png'), bbox_inches='tight', dpi=300)
    plt.savefig(os.path.join(output_dir, 'heatmaps.pdf'), bbox_inches='tight')
    plt.close()

def print_plot_summary():
    """Print summary of what each plot shows"""
    print("\n" + "="*70)
    print("GDPR LOGGER BENCHMARK ANALYSIS - PLOT SUMMARY")
    print("="*70)
    
    plots = [
        ("encryption_effect.png", 
         "4-panel analysis of encryption impact across: (a) batch sizes, (b) entry sizes, (c) write amplification vs compression, (d) writer threads. Shows encryption overhead in different scenarios."),
        
        ("compression_effect.png", 
         "4-panel analysis of compression impact across: (a) batch sizes, (b) entry sizes, (c) write amplification vs encryption, (d) writer threads. Shows compression efficiency tradeoffs."),
        
        ("entry_size_effect.png", 
         "4-panel analysis of entry size impact across: (a) batch sizes, (b) data throughput vs compression, (c) encryption settings, (d) writer threads. Shows size vs performance relationships."),
        
        ("writer_threads_effect.png", 
         "4-panel analysis of writer threads scaling: (a) throughput vs batch sizes, (b) throughput vs encryption/compression combinations, (c) scaling efficiency by entry size, (d) latency impact."),
        
        ("batch_size_effect.png", 
         "4-panel analysis of batch size impact: (a) throughput vs encryption/compression combinations, (b) throughput vs entry sizes, (c) latency vs writer threads, (d) write amplification trends."),
        
        ("heatmaps.png", 
         "4 heatmaps showing: (a) throughput vs batch size/compression, (b) write amplification vs entry size/encryption, (c) throughput vs writers/entry size, (d) latency vs batch size/writers."),
        
        ("batch_analysis.png", 
         "2-panel analysis of batch size impact (with encryption=ON and compression=OFF): (a) throughput vs batch size, (b) write amplification vs batch size.")
    ]
    
    for i, (filename, description) in enumerate(plots, 1):
        print(f"\n{i}. {filename}:")
        print(f"   {description}")
    
    print(f"\n{'='*70}")
    print("KEY FEATURES:")
    print("- Linear x-axis scales with actual values labeled")
    print("- All parameter combinations shown for each main effect")
    print("- Line plots for continuous relationships")
    print("- Bar plots for categorical comparisons")
    print("- Multiple heatmaps for interaction effects")
    print("- Detailed legends and consistent color schemes")
    print(f"{'='*70}")

def main():
    parser = argparse.ArgumentParser(description="Generate GDPR logger benchmark analysis plots")
    parser.add_argument("--input_file", type=str, default="gdpr_logger_benchmark_results.csv",
                       help="Input CSV file with benchmark results")
    parser.add_argument("--output_dir", type=str, default="gdpr_plots",
                       help="Directory to save the generated plots")
    args = parser.parse_args()
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Load and preprocess data
    print(f"Loading data from {args.input_file}...")
    df = load_gdpr_data(args.input_file)
    print(f"Loaded {len(df)} benchmark results")
    
    print(f"Data summary:")
    print(f"  - Batch sizes: {sorted(df['batch_size'].unique())}")
    print(f"  - Entry sizes: {sorted(df['entry_size_bytes'].unique())}")
    print(f"  - Writer threads: {sorted(df['consumers'].unique())}")
    print(f"  - Encryption settings: {sorted(df['use_encryption'].unique())}")
    print(f"  - Compression levels: {sorted(df['compression_level'].unique())}")
    
    # Generate all plots
    print("Generating plots...")
    # create_encryption_effect_plots(df, args.output_dir)
    # create_compression_effect_plots(df, args.output_dir)
    # create_entry_size_effect_plots(df, args.output_dir)
    # create_writer_threads_effect_plots(df, args.output_dir)
    # create_batch_size_effect_plots(df, args.output_dir)
    # create_heatmaps(df, args.output_dir)
    create_encryption_batch_analysis_plot(df, args.output_dir)
    
    print(f"All plots saved to {args.output_dir}/")
    
    # Print summary
    print_plot_summary()

if __name__ == "__main__":
    main()
