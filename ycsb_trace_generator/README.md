# YCSB and GDPR workload trace generator

### Important files and usage
The [`workload_generator.sh`](./workload_generator.sh) script generates the workload traces.
Before running the script, adapt the `TRACE_FOLDER` variable to point to the directory where your traces want to be stored.
To generate the workloads run:
```
./workload_generator.sh
```
This script executes all the workloads (core YCSB and GDPR workloads) and generates the respective traces.
Note that, to limit the size of the resulting files, the values are marked with the placeholder VAL.
If you want to have actual, random generated values in the workload traces, set `mockvalues=false` in the workload configuration files.

**Important files:**
1. The workload configuration files are placed [here](https://github.com/dimstav23/GDPRbench/tree/master/src/tracer_workloads).
2. The dummy tracer client is located [here](https://github.com/dimstav23/GDPRbench/tree/master/src/tracer/src/main/java/com/yahoo/ycsb/db/TracerClient.java)
3. The adapted GDPR workload class is located [here](https://github.com/dimstav23/GDPRbench/tree/master/src/core/src/main/java/com/yahoo/ycsb/workloads/GDPRWorkload.java)
4. For comparison, the default Core workload class is [here](https://github.com/dimstav23/GDPRbench/tree/master/src/core/src/main/java/com/yahoo/ycsb/workloads/CoreWorkload.java) 
and the sample Redis Client provided by GDPRBench is [here](https://github.com/dimstav23/GDPRbench/tree/master/src/redis/src/main/java/com/yahoo/ycsb/db/RedisClient.java)

### GDPR metadata
GDPRuler metadata fields and their associated enums in the code:
1. Expiration time: `TTL`
2. Purpose: `PUR`
3. Origin: `SRC`
4. Sharing: `SHR`
5. Objections: `OBJ`
6. Monitoring: `LOG`
7. Data Owner: `USR`

**Note:**
The origin(`SRC`) and data owner(`USR`) are currently considered immutable fields, which means that they cannot be modified through `PUTM` queries.

### Differences compared to GDPRBench workloads
1. in YCSB core workloads, keys are prefixed with the string "user" (https://github.com/GDPRbench/GDPRbench/blob/94d399c099de9819b493fd5e4b553b54376dcead/src/core/src/main/java/com/yahoo/ycsb/workloads/CoreWorkload.java#L526)
2. in GDPR core workloads, keys are prefixed with the string "key" (https://github.com/GDPRbench/GDPRbench/blob/94d399c099de9819b493fd5e4b553b54376dcead/src/core/src/main/java/com/yahoo/ycsb/workloads/GDPRWorkload.java#L647)
3. the Core workload values are the fields generated concatenated into a String (default: 10 fields, 100 bytes each -> 1000bytes)
   while the GDPR workload values are set via the `fieldlength` parameter in the workload file
4. for the Update operation in the Core workloads, we set the `writeallfields` to update all the fields of the value 
5. Removed `DEC` and `ACL` metadata from GDPRBench since they are implied in the rest (respective *count* variables are removed from the workload files)
6. Converted `CAT` to `LOG` metadata for monitoring with `true`/`false` values (respective *count* variables are removed from the workload files)
7. Reduced the default number of fieldcounts to 8 from 10 (due to metadata field reduction).
8. `mockvalues` parameter in the workload configuration instructs the tracer not to log the random values but place a `VAL` placeholder
9. `verifyTTL` query removed from GDPR workload / should be done after the development of the controller to verify the correctness
10. `getLogs()` query specifies the amount of records that need to be audited / will be enhanced in our framework, just keep it to know when to call it
11. `UPDATEMETAPURPOSE` `PUTM` requests, should also be accompanied with the `USR ID` that requests the modification to see where he has access to (when the query filter is the purpose)
12. `SHR` field contains the `USR ID` with whom the data is shared
13. `OBJ` field contains the objected purposes of use for this specific KV pair

### Further information
For more information on the sequence of execution phases for the YCSB core workloads
see [here](https://github.com/brianfrankcooper/YCSB/wiki/Core-Workloads#running-the-workloads).

**Note:** We perform a separate load phase for each workload to have autonomous workload traces
Load A Run A, Load A Run B, Load A Run C, Load A Run F, Load A Run D, Load E Run E

For GDPR workloads we load and run the workloads as described
[here](https://github.com/GDPRbench/GDPRbench#benchmarking).
