# Testing notes

## Filter test
The `filter_test_trace` contains a test workloads to validate the GDPR metadata filtering.

To run the workload:
1. Start a redis server. For example, go to the root directory and execute:
```
./KVs/redis/src/redis-server --protected-mode no
```

2. At the root directory of the project, execute the following:
```
python3 scripts/GDPRuler.py --config ./configs/test_user.json --workload ./workload_traces/filter_test_trace --db redis`
```

Expected query output of first run:
```
GET operation failed
query purposes not in the allowed purposes of use of the KV pair
query purposes not in the allowed purposes of use of the KV pair
GET operation failed
GET operation failed
client key not in the owner/share groups of the KV pair
GET operation failed
query purposes in the objections of the KV pair
query purposes not in the allowed purposes of use of the KV pair
client key not in the owner/share groups of the KV pair
GET operation failed
Monitor required
GET operation failed
Monitor required
```

Expected query output of the second run (after > 1s):
```
query purposes not in the allowed purposes of use of the KV pair
query purposes not in the allowed purposes of use of the KV pair
client key not in the owner/share groups of the KV pair
client key not in the owner/share groups of the KV pair
query purposes in the objections of the KV pair
query purposes not in the allowed purposes of use of the KV pair
client key not in the owner/share groups of the KV pair
Monitor required
Monitor required
expired KV pair
expired KV pair
```