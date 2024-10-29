import pandas as pd
import matplotlib.pyplot as plt
import os
import re
import argparse

# Define the function to read files and extract information based on filenames
def load_data_from_directory(input_dir):
    pattern = r"(?P<controller>\w+)-(?P<workload_type>.*?_\d+M)-encryption_(?P<encryption>\w+)-logging_(?P<logging>\w+)"
    data = []
    
    for filename in os.listdir(input_dir):
        if filename.endswith(".csv"):
            match = re.match(pattern, filename)
            if match:
                controller = match.group("controller")
                workload_type = match.group("workload_type")
                encryption = match.group("encryption")
                logging = match.group("logging")

                # Read the CSV and add columns for metadata
                df = pd.read_csv(os.path.join(input_dir, filename))
                
                # Extract operation count from workload name
                operation_count = int(workload_type.split("_")[-1].replace("M", "000000"))
                
                # Calculate throughput
                df['throughput'] = operation_count / df['elapsed_time (s)']
                
                # Add metadata columns
                df['controller'] = controller
                df['workload_type'] = workload_type
                df['encryption'] = encryption
                df['logging'] = logging
                df['operation_count'] = operation_count
                # Extract workload starting with 'workload' from the first column
                df['workload'] = df.iloc[:, 0].str.extract(r'^(workload[a-f])')[0]

                data.append(df)
    
    return pd.concat(data, ignore_index=True)

# Function to generate and save annotated plots
def plot_and_save(data, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    for (db, encryption, logging), group_data in data.groupby(['db', 'encryption', 'logging']):
        fig, axes = plt.subplots(2, 1, figsize=(14, 12))

        # Prepare x-axis values (unique client numbers mapped to indices)
        n_clients = sorted(group_data['n_clients'].unique())
        workloads = sorted(group_data['workload'].unique())
        controllers = sorted(group_data['controller'].unique())

        num_workloads = len(workloads)
        num_controllers = len(controllers)
        num_of_bars = num_workloads * num_controllers
        bar_width = 0.8 / num_of_bars  # Adjust bar width based on total bars

        # Group data by controller and workload type to ensure correct plotting
        group_data['x_idx'] = group_data['n_clients'].map({client: idx for idx, client in enumerate(n_clients)})

        # Latency plot
        ax = axes[0]
        for i, (workload, workload_data) in enumerate(group_data.groupby('workload')):
            for j, (controller, controller_data) in enumerate(workload_data.groupby('controller')):
                offset = (-(num_of_bars / 2) + j - (len(workload_data['controller'].unique()) - 1) / 2) * bar_width + (i * (bar_width + 0.1)) + bar_width / 2
                
                # Convert latency from seconds to nanoseconds
                latencies_us = controller_data['avg_latency (s)'] * 1_000_000
                
                bars = ax.bar(controller_data['x_idx'] + offset,
                              latencies_us, width=bar_width, 
                              label=f"{controller} - {workload}", alpha=0.7)

                # Annotate each bar with its height (4 decimal points, vertical text)
                for bar in bars:
                    yval = bar.get_height()
                    ax.text(bar.get_x() + bar.get_width() / 2, yval, f'{yval:.2f}', 
                            ha='center', va='bottom', fontsize=10, rotation=90)

        ax.set_xticks(range(len(n_clients)))
        ax.set_xticklabels(n_clients)
        ax.set_xlabel('Number of Clients')
        ax.set_ylabel('Average Latency (us)')  # Change label to ns
        ax.set_title(f'Latency - DB: {db}, Encryption: {encryption}, Logging: {logging} (Lower is Better)')
        ax.legend()

        # Throughput plot
        ax = axes[1]
        for i, (workload, workload_data) in enumerate(group_data.groupby('workload')):
            for j, (controller, controller_data) in enumerate(workload_data.groupby('controller')):
                offset = (-(num_of_bars / 2) + j - (len(workload_data['controller'].unique()) - 1) / 2) * bar_width + (i * (bar_width + 0.1)) + bar_width / 2
                
                # Convert throughput from ops/sec to kops
                throughputs_kops = controller_data['throughput'] / 1000  # Convert to kilops

                bars = ax.bar(controller_data['x_idx'] + offset,
                              throughputs_kops, width=bar_width, 
                              label=f"{controller} - {workload}", alpha=0.7)

                # Annotate each bar with its height (2 decimal points, vertical text)
                for bar in bars:
                    yval = bar.get_height()
                    ax.text(bar.get_x() + bar.get_width() / 2, yval, f'{yval:.2f}', 
                            ha='center', va='bottom', fontsize=10, rotation=90)

        ax.set_xticks(range(len(n_clients)))
        ax.set_xticklabels(n_clients)
        ax.set_xlabel('Number of Clients')
        ax.set_ylabel('Throughput (kops)')  # Change label to kops
        ax.set_title(f'Throughput - DB: {db}, Encryption: {encryption}, Logging: {logging} (Higher is Better)')
        ax.legend()
        
        # Save plots
        filename_base = f"{db}_encryption_{encryption}_logging_{logging}"
        plt.tight_layout()
        fig.savefig(os.path.join(output_dir, f"{filename_base}.png"))
        fig.savefig(os.path.join(output_dir, f"{filename_base}.pdf"))
        plt.close(fig)

def main():
    parser = argparse.ArgumentParser(description="Generate latency and throughput plots from CSV data.")
    parser.add_argument("--input_dir", type=str, default="../bare_metal/results", help="Directory containing the CSV files")
    parser.add_argument("--output_dir", type=str, default="plots", help="Directory to save the generated plots")
    args = parser.parse_args()

    # Load data from specified directory
    data = load_data_from_directory(args.input_dir)

    # Generate and save plots
    plot_and_save(data, args.output_dir)

if __name__ == "__main__":
    main()
