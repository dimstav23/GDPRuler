import matplotlib.pyplot as plt

def parse_file(file):
    data = {}
    with open(file) as f:
        for line in f:
            parts = line.strip().split(',')
            workload = parts[0]
            time_type = parts[1]
            avg_time = float(parts[2])
            
            if workload not in data:
                data[workload] = {}
            
            data[workload][time_type] = avg_time
    return data

def plot_overhead(vanilla_data, native_gdpr_data, secure_vanilla_data, secure_gdpr_data):
    workloads = list(vanilla_data.keys())
    workloads.sort()
    
    native_gdpr = [100 * (native_gdpr_data[workload]['system_time'] - vanilla_data[workload]['system_time']) / vanilla_data[workload]['system_time'] for workload in workloads]
    secure_vanilla = [100 * (secure_vanilla_data[workload]['system_time'] - vanilla_data[workload]['system_time']) / vanilla_data[workload]['system_time'] for workload in workloads]
    secure_gdpr = [100 * (secure_gdpr_data[workload]['system_time'] - vanilla_data[workload]['system_time']) / vanilla_data[workload]['system_time'] for workload in workloads]
    
    fig, ax = plt.subplots()
    ax.bar(workloads, native_gdpr, label='Native GDPR')
    ax.bar(workloads, secure_vanilla, label='Secure Vanilla', bottom=native_gdpr)
    ax.bar(workloads, secure_gdpr, label='Secure GDPR', bottom=[x+y for x, y in zip(native_gdpr, secure_vanilla)])
    ax.set_ylabel('Overhead %')
    ax.set_xlabel('Workload')
    ax.legend()
    plt.savefig('overhead_plot.png')

def main():
    vanilla_data = parse_file('./results/native_average.csv')
    native_gdpr_data = parse_file('./results/gdpr_average.csv')
    secure_vanilla_data = parse_file('./results/sec_vanilla_average.csv')
    secure_gdpr_data = parse_file('./results/sec_gdpr_average.csv')

    plot_overhead(vanilla_data, native_gdpr_data, secure_vanilla_data, secure_gdpr_data)

    return

if __name__ == "__main__":
    main()