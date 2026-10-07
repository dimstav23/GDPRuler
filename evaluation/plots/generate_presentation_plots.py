"""
Slide-sized variants of the paper plots for the GDPRuler talk (GDPRuler-CCS26 deck).

Each figure is rendered at the size of its placeholder in the deck, so it can be
inserted at 100% scale (the logging plot is slightly wider than its placeholder):
  - slide 26: ycsb_single_thread_comparison          (9.10 x 3.40 in)
  - slide 27: gdpr_metadata_indexes_throughput_redis (9.10 x 3.40 in)
  - slide 28: logging_impact_rocksdb                 (5.20 x 3.40 in)
  - slide 34: ycsb_workloada_scalability_rocksdb     (9.10 x 3.40 in)

Data loading and aggregation are reused from the paper plotting scripts so the
numbers are identical to the paper figures.
"""
import argparse
import os

import numpy as np
import pandas as pd
import matplotlib as mpl
import matplotlib.pyplot as plt

import generate_ycsb_performance_plot as ycsb_plots
import generate_gdpr_queries_plots as gdpr_queries_plots
import generate_logging_plot_and_stats as logging_plots

# The paper scripts set seaborn/paper styles on import; start from plain matplotlib defaults
mpl.rcdefaults()
mpl.use("Agg")
mpl.rcParams.update({
    "font.size": 14,
    "axes.titlesize": 15,
    "axes.labelsize": 14,
    "xtick.labelsize": 13,
    "ytick.labelsize": 13,
    "legend.fontsize": 14,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": False,
})

# Placeholder sizes (inches) in the GDPRuler-CCS26 slides
SLIDE_WIDE = (9.10, 3.40)
# Slightly wider than the 4.45in placeholder on slide 28
SLIDE_HALF = (5.20, 3.40)

TITLE_COLOR = "navy"
HIGHER_IS_BETTER = r"($\bf{Higher\ is\ better↑}$)"
ERRORBAR_KW = dict(fmt='none', ecolor='black', capsize=3, lw=1)

# (variant prefix, legend label, colour) -- default matplotlib colour cycle
DB_NAMES = {'redis': 'Redis', 'rocksdb': 'RocksDB'}

YCSB_VARIANTS = [
    ("direct_bare_metal-no_encr", "KVS", "C0"),
    ("direct_CVM-no_encr", "CVM KVS", "C1"),
    ("gdpr_CVM-encr", "GDPRuler", "C2"),
]


def save(fig, output_dir, name):
    os.makedirs(output_dir, exist_ok=True)
    # No bbox_inches='tight': keep the exact placeholder dimensions
    fig.savefig(os.path.join(output_dir, f"{name}.png"), dpi=300)
    plt.close(fig)
    print(f"  ✓ {name}.png")


def load_ycsb_data(bare_metal_dir, vm_dir):
    data = pd.concat([ycsb_plots.load_data_from_directory(bare_metal_dir),
                      ycsb_plots.load_data_from_directory(vm_dir)], ignore_index=True)
    return ycsb_plots.filter_data_for_ycsb_performance_plot(data)


def plot_ycsb_bars(ax, df, group_by, x_labels):
    """Grouped bars (one group per x value) for the YCSB_VARIANTS."""
    x = np.arange(len(x_labels))
    width = 0.8 / len(YCSB_VARIANTS)
    for j, (prefix, label, color) in enumerate(YCSB_VARIANTS):
        df_var = df[df['variant'].str.startswith(prefix)]
        values, errors = ycsb_plots.prepare_plot_data(df_var, 'throughput', group_by)
        pos = x + width * j - 0.4 + width / 2
        ax.bar(pos, values, width, label=label, color=color, edgecolor='black', linewidth=0.8)
        ax.errorbar(pos, values, yerr=errors, **ERRORBAR_KW)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.tick_params(axis='x', length=0)


def create_single_thread_comparison(data, output_dir):
    """Slide 26: Redis & RocksDB, 1 thread, all workloads (KVS / CVM KVS / GDPRuler)."""
    data_1thread = data[data['n_clients'] == 1]
    workloads = sorted(data_1thread['workload'].unique())

    fig, axes = plt.subplots(1, 2, figsize=SLIDE_WIDE, sharey=True)
    for ax, db in zip(axes, ['redis', 'rocksdb']):
        plot_ycsb_bars(ax, data_1thread[data_1thread['db'] == db], 'workload',
                       [w[-1].upper() for w in workloads])
        ax.set_title(f"{DB_NAMES[db]} {HIGHER_IS_BETTER}", color=TITLE_COLOR)
        ax.set_xlabel('YCSB Workload')
    axes[0].set_ylabel('Throughput (kops/s)')
    axes[1].tick_params(axis='y', labelleft=True)

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', ncol=len(labels), frameon=False,
               bbox_to_anchor=(0.5, 1.0))
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    save(fig, output_dir, "presentation_ycsb_single_thread_comparison")


def create_workloada_scalability(data, output_dir):
    """Slide 34: RocksDB, workload A, 1-16 threads (KVS / CVM KVS / GDPRuler)."""
    df = data[(data['workload'] == 'workloada') & (data['db'] == 'rocksdb')]
    thread_counts = sorted(df['n_clients'].unique())

    fig, ax = plt.subplots(figsize=SLIDE_WIDE)
    plot_ycsb_bars(ax, df, 'n_clients', thread_counts)
    ax.set_title(f"RocksDB – Workload A (50/50 R/W) {HIGHER_IS_BETTER}", color=TITLE_COLOR)
    ax.set_xlabel('Threads')
    ax.set_ylabel('Throughput (kops/s)')
    ax.legend(loc='upper left', frameon=False)
    fig.tight_layout()
    save(fig, output_dir, "presentation_ycsb_workloada_scalability_rocksdb")


def create_gdpr_metadata_indexes_throughput(data, output_dir, db_name='redis'):
    """Slide 27: GDPR workload throughput, GDPRuler vs GDPRuler w/ metadata indexes."""
    df = data[(data['encryption'] == 'ON') & (data['db'] == db_name) &
              (data['environment'] == 'CVM')]
    entities = sorted(df['entity'].unique())
    variants = [(False, 'GDPRuler', 'C0'), (True, 'GDPRuler (w/ indexes)', 'C1')]

    x = np.arange(len(entities))
    width = 0.3
    offsets = [-0.16, 0.16]
    means = {}

    fig, ax = plt.subplots(figsize=SLIDE_WIDE)
    for (has_indexes, label, color), offset in zip(variants, offsets):
        grouped = df[df['metadata_indexes'] == has_indexes].groupby('entity')['throughput']
        mean, std = grouped.mean().reindex(entities), grouped.std().reindex(entities)
        means[has_indexes] = mean
        pos = x + offset
        ax.bar(pos, mean, width, label=label, color=color, edgecolor='black', linewidth=0.8)
        ax.errorbar(pos, mean, yerr=std, **ERRORBAR_KW)

    # Speedup arrows on top of the base bar, reaching the indexed bar's height
    for i, entity in enumerate(entities):
        base, improved = means[False][entity], means[True][entity]
        x_arrow = x[i] + offsets[0]
        ax.annotate('', xy=(x_arrow, improved), xytext=(x_arrow, base),
                    arrowprops=dict(arrowstyle='<->', color='darkblue', lw=1.5,
                                    shrinkA=0, shrinkB=0))
        ax.text(x_arrow - 0.03, np.sqrt(base * improved), f'{improved / base:.1f}×',
                ha='right', va='center', fontsize=14, color='darkblue', fontweight='bold')

    ax.set_yscale('log')
    all_values = np.concatenate([m.values for m in means.values()])
    ax.set_ylim(all_values.min() * 0.5, all_values.max() * 4)
    ax.set_xticks(x)
    ax.set_xticklabels([e.capitalize() for e in entities])
    ax.tick_params(axis='x', length=0)
    ax.set_xlim(-0.6, len(entities) - 0.4)
    ax.set_title(f"{DB_NAMES[db_name]} – GDPR workload throughput {HIGHER_IS_BETTER}",
                 color=TITLE_COLOR)
    ax.set_xlabel('GDPR Workload')
    ax.set_ylabel('Throughput (ops/s)')
    ax.legend(loc='upper right', frameon=False)
    fig.tight_layout()
    save(fig, output_dir, f"presentation_gdpr_metadata_indexes_throughput_{db_name}")


def create_logging_impact(data, output_dir, db_name='rocksdb'):
    """Slide 28: throughput vs. percentage of logged KV pairs for a single KVS."""
    df = data[data['db'] == db_name]
    workload_names = sorted(df['workload_name'].unique())
    logged_percentages = sorted(df['logged_percent'].unique())

    x = np.arange(len(workload_names))
    width = 0.8 / len(logged_percentages)

    fig, ax = plt.subplots(figsize=SLIDE_HALF)
    for i, percent in enumerate(logged_percentages):
        stats = [logging_plots.prepare_plot_data_logging(df, w, percent) for w in workload_names]
        values, errors = zip(*stats)
        pos = x + width * i - 0.4 + width / 2
        ax.bar(pos, values, width, label=f'{percent}%', color=f'C{i}',
               edgecolor='black', linewidth=0.8)
        ax.errorbar(pos, values, yerr=errors, **ERRORBAR_KW)

    ax.set_xticks(x)
    ax.set_xticklabels([f"Workload {w[-1].upper()}" for w in workload_names])
    ax.tick_params(axis='x', length=0)
    ax.set_ylabel('Throughput (kops/s)')
    ax.set_title(f"{DB_NAMES[db_name]} {HIGHER_IS_BETTER}", color=TITLE_COLOR)

    handles, labels = ax.get_legend_handles_labels()
    fig.legend(handles, labels, title='Percentage of logged KV pairs', loc='upper center',
               ncol=len(labels), frameon=False, bbox_to_anchor=(0.5, 1.0),
               fontsize=13, title_fontsize=13, columnspacing=1.0, handletextpad=0.4)
    fig.tight_layout(rect=(0, 0, 1, 0.86))
    save(fig, output_dir, f"presentation_logging_impact_{db_name}")


def main():
    parser = argparse.ArgumentParser(description="Generate slide-sized plots for the GDPRuler presentation")
    parser.add_argument("--bare_metal_results", type=str, default="../bare_metal/results", help="Directory containing the bare metal CSV files")
    parser.add_argument("--vm_results", type=str, default="../VM/results", help="Directory containing the VM CSV files")
    parser.add_argument("--output_dir", type=str, default="plots", help="Directory to save the generated plots")
    args = parser.parse_args()

    ycsb_data = load_ycsb_data(args.bare_metal_results, args.vm_results)
    gdpr_data = gdpr_queries_plots.load_data_from_dirs(args.bare_metal_results, args.vm_results)
    logging_data = logging_plots.load_logging_data(args.vm_results, variant="gdpr_CVM",
                                                   n_clients=8, encryption="ON")

    print("\nGenerating presentation plots...")
    create_single_thread_comparison(ycsb_data, args.output_dir)
    create_gdpr_metadata_indexes_throughput(gdpr_data, args.output_dir, 'redis')
    create_logging_impact(logging_data, args.output_dir, 'rocksdb')
    create_workloada_scalability(ycsb_data, args.output_dir)


if __name__ == "__main__":
    main()
