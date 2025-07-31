# GDPRuler microbenchmarks

## Storage and Network microbenchmarks based on CVM_eval
After you initialize the `CVM_eval` submodule,
simply run from the current directory:
```
bash run_CVM_eval_benchmarks.sh
```

This script will:
1. set up a separate image (similar to the one used by `GDPRuler`),
2. set up the networking required for the experiments
3. run the `CVM_eval` storage and network experiments
4. plot the results in the end