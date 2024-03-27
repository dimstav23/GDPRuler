import csv
import os

def aggregate_csv_files(input_files, output_file, client_num):
  # Initialize aggregate data
  total_time_ms = 0.0
  total_throughput_ops_s = 0.0
  total_avg_latency_ms = 0.0
  avg_latency_get_ms = 0.0
  max_latency_get_ms = 0.0
  avg_latency_put_ms = 0.0
  max_latency_put_ms = 0.0
  avg_latency_del_ms = 0.0
  max_latency_del_ms = 0.0
  
  num_files_processed = 0

  # Iterate through each input file
  for input_file in input_files:
    if not os.path.isfile(input_file):
      print(f"File '{input_file}' does not exist.")
      continue

    with open(input_file, 'r') as f:
      reader = csv.reader(f)
      found_metric_value_line = False

      # Iterate through each line in the file
      for row in reader:
        if found_metric_value_line:
          if row[0] == "Total client time (ms)":
            total_time_ms += float(row[1])
          elif row[0] == "Total client throughput (ops/s)":
            total_throughput_ops_s += float(row[1])
          elif row[0] == "Average client latency (ms)":
            total_avg_latency_ms += float(row[1])
          elif row[0] == "Average client latency for get (ms)":
            avg_latency_get_ms += float(row[1])
          elif row[0] == "Maximum client latency for get (ms)":
            max_latency_get_ms = max(max_latency_get_ms, float(row[1]))
          elif row[0] == "Average client latency for put (ms)":
            avg_latency_put_ms += float(row[1])
          elif row[0] == "Maximum client latency for put (ms)":
            max_latency_put_ms = max(max_latency_put_ms, float(row[1]))
          elif row[0] == "Average client latency for del (ms)":
            avg_latency_del_ms += float(row[1])
          elif row[0] == "Maximum client latency for del (ms)":
            max_latency_del_ms = max(max_latency_del_ms, float(row[1]))
        elif row[0] == "Metric" and row[1] == "Value":
          found_metric_value_line = True

    num_files_processed += 1

  # Calculate averages
  num_files_processed = max(1, num_files_processed)  # Ensure we don't divide by zero
  total_time_avg = total_time_ms / num_files_processed # Average execution time per client
  total_throughput_avg = total_throughput_ops_s / num_files_processed * client_num # client_num to get a proper throughput for all the clients
  total_avg_latency_avg = total_avg_latency_ms / num_files_processed # average latency among client's requests for the whole experiment
  avg_latency_get_avg = avg_latency_get_ms / num_files_processed
  avg_latency_put_avg = avg_latency_put_ms / num_files_processed
  avg_latency_del_avg = avg_latency_del_ms / num_files_processed

  # Write aggregated data to the output file
  with open(output_file, 'w') as f:
    writer = csv.writer(f)
    writer.writerow(["Metric", "Value"])
    writer.writerow(["Total avg time per client (ms)", total_time_avg])
    writer.writerow(["Total throughput (ops/s)", total_throughput_avg])
    writer.writerow(["Average latency (ms)", total_avg_latency_avg])
    writer.writerow(["Average latency for get (ms)", avg_latency_get_avg])
    writer.writerow(["Maximum latency for get (ms)", max_latency_get_ms])
    writer.writerow(["Average latency for put (ms)", avg_latency_put_avg])
    writer.writerow(["Maximum latency for put (ms)", max_latency_put_ms])
    writer.writerow(["Average latency for del (ms)", avg_latency_del_avg])
    writer.writerow(["Maximum latency for del (ms)", max_latency_del_ms])

def delete_temp_csv_files(temp_csv_files):
  for csv_file in temp_csv_files:
    os.unlink(csv_file)