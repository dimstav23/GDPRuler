# Print utilities for GDPRuler Logger benchmark

You can see the benchmark configuration [here](../benchmarks/workloads/gdpruler.cpp).

Make sure to execute the benchmark `gdpruler_benchmark` that is produced during the building process
of the benchmarks of the `gdpr-logger` repository.
This should produce a `csv` in its respective folder with the name `gdpr_logger_benchmark_results.csv`.

To produce the plots, simply run:
```
python3 gdpruler_benchmark_plot.py --input_file ../build/gdpr_logger_benchmark_results.csv
```