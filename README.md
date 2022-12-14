# GDPRuler

### Repository structure
[sev_demo](./sev_demo): Folder containing documentation for setting up an ubuntu based AMD SEV VM and perform proof tests.

[policy_compiler](./policy_compiler): Folder containing the policy compiler.

[configs](./configs): Folder containing sample configs for data owner, data controller, data processor (3rd party) and regulator.

[controller](./controller): Folder containing the core code of the data controller.

[KVs](./KVs): Folder containing the KVs submodules.

[sev-tool](./sev-tool/): submodule providing AMD SEV functionalities

[ycsb_trace_generator](./ycsb_trace_generator/): submodule containing a modified version of GDPRBench (YCSB-based) to produce workload traces