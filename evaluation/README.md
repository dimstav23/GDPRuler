# GDPRuler evaluation

## Plain YCSB workloads

### Native (bare-metal) execution
For the plain YCSB workloads (workload A-F) that enforce the default (generated) client policies 
(see [the policy generator script](./default_policy_creator.sh)),
you can run the following to run all the variants with and without encryption and logging enabled:
```
$ cd bare-metal
$ ./automated_runner.sh
```
The configuration parameters are taken from [`bare_metal/config.sh`](./bare_metal/config.sh).

Overall, the [`automated_runner.sh`](./bare_metal/automated_runner.sh) 
The underlying scripts (`native_direct.sh`, `native_ctl.sh`, `gdpr_ctl.sh`) perform the following actions with the help of the [`common.sh`](./common.sh) and [`args_and_checks.sh`](./args_and_checks.sh) scripts:
- Parses the `--encryption` argument (default set to `OFF`) and compiles the controller
- Parses the `--logging` argument (default set to `OFF`) and generates the client configuration files with the value of the `monitor` field set to `true` or `false` depending on the chosen option.
- Sets the appropriate parameters for the controller and db address and ports
- Creates a `results` directory, if it does not exist
- Performs loops that run over the configured set of parameters and perform the experiments
- For each controller type (`direct` aka no proxy controller,`native`, `gdpr`), it stores the results in .csv files in the `results` directory with the name
convetion of:

`[controller_type]-query_mgmt-encryption_[ON/OFF]-logging_[ON/OFF].csv`

- In the end, it prints a summary of the failed tests, if any.

### VM-based execution (outdated)
In this setup, the DB server and the controller are executed inside a confidential VM (currently SEV-ES).
For the plain YCSB workloads (workload A-F) that enforce the default (generated) client policies 
(see [the policy generator script](./default_policy_creator.sh)),
you can run the following, depending on the desired encryption and logging settings:
```
$ cd VM
$ ./run_query_mgmt.sh --encryption OFF --logging OFF
$ ./run_query_mgmt.sh --encryption OFF --logging ON
$ ./run_query_mgmt.sh --encryption ON --logging OFF
$ ./run_query_mgmt.sh --encryption ON --logging ON
```

Overall, the [`run_query_mgmt.sh`](./VM/run_query_mgmt.sh) script performs the following actions with the help of the [`common.sh`](./common.sh) and [`args_and_checks.sh`](./args_and_checks.sh) scripts:
- Parses the `--encryption` argument (default set to `OFF`) and compiles the controller inside the controller VM with the appropriate option.
- Parses the `--logging` argument (default set to `OFF`) and generates the client configuration files with the value of the `monitor` field set to `true` or `false` depending on the chosen option. These configuration files are copied inside the controller VM.
- Sets the appropriate parameters for the controller and db address and ports
- Creates a `results` directory, if it does not exist
- Performs loops that run over the configured set of parameters and perform the experiments. In this setup, the spawn of the VMs is performed through the `expect` scripts ([`CVM_GDPRuler.expect`](./VM/CVM_GDPRuler.expect) and [`VM_server.expect`](./VM/VM_server.expect)).
- For each controller type (`native`, `gdpr`), it stores the results in .csv files in the `results` directory with the name
convetion of:

`[controller_type]-query_mgmt-encryption_[ON/OFF]-logging_[ON/OFF].csv`

- In the end, it prints a summary of the failed tests, if any.

---

**Notes**:
- For more information about the scripts, please see [here](./DOCUMENTATION.md).