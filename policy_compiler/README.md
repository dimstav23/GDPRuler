# GDPRuler policy-compiler

## Supported predicates
- `eq(x,y)`, `le(x,y)`, `lt(x,y)` | x = y, x ≤ y, x < y 
- `ge(x,y)`, `gt(x,y)` | x ≥ y, x > y
- `and(x,y)` | x & y (*logical and*)
- `sessionKeyIs(Kuser)`: verifies that the session user has access to the data
- `purKeyIs(Kuser)`: gets the purpose of use for this user
- `monitor(k)`: log all the operations associated with the key k
- `objExpT(k)`: get the KV expiry time
- `objPur(k)`: get the KV purpose(s) of use
- `objOrig(k)`: get the origin of the KV pair
- `objShare(k)`: get the sharing information of the KV pair
- `objObjections(k)`: get the objections imposed on a KV pair

Optional for processing activity (can be encapsulated to purposes & objections):
- `locIs(loc)`: predicate to assure the location of the compute node
- `engineVersion(v)` : predicate to assure the engine version 

## GDPR rules

### #5 Purpose limitation: Collect data for explicit purposes
read :− sessionKeyIs(Kauth_user) & eq(purKeyIs(Kauth_user) **and** objPur(k), purKeyIs(Kauth_user))

### #5 Storage limitation: Do not store data indefinitely
read :− sessionKeyIs(Kauth_user) & le(time, objExpT(k))

### #13, #14 Information to be provided: provide GDPR metadata
read :- objExpT(k) & objPur(k) & objOrig(k) & objShare(k)

### #15 Right of access by users: users access all their data
read :- sessionKeyIs(Kauth_user)
update :- sessionKeyIs(Kauth_user)

### #17 Right to be forgotten: Allow customers to erase their data
delete :- sessionKeyIs(Kauth_user)

### #21 Right to object: Do not use data for objected reasons
read :− sessionKeyIs(Kauth_user) & eq(purKeyIs(Kauth_user) **and** objObjections(k),0)

### #22 Automated individual decision-making
Can be deducted through #5 and #21 predicates.

### #25 Data protection by design and default - access control
read :- sessionKeyIs(Kauth_user)
update :- sessionKeyIs(Kauth_user)
delete :- sessionKeyIs(Kauth_user)

### #28 Processor: Do not grant unlimited access to data
read :- sessionKeyIs(Kauth_user)
update :- sessionKeyIs(Kauth_user)
delete :- sessionKeyIs(Kauth_user)

### #30 Records of processing activity
read :- sessionKeyIs(Kauth_user) & monitor(k)
update :- sessionKeyIs(Kauth_user) & monitor(k)
delete :- sessionKeyIs(Kauth_user) & monitor(k)

### #32 Security of processing
- Encryption of the data, metadata and logs
- Secure execution environment (e.g., SEV)

### #33 Personal data breach: Share logs from affected systems
read :- sessionKeyIs(Kauth_user) & monitor(k)


### Script documentation

[policy_config.py](./policy_config.py): takes as an argument a policy configuration (e.g. the [owners policy config](../configs/owner_policy.json)), 
parses the json file and returns a list of arguments to be provided to the controller to setup the user information. The output format is
`user_policy -policy_attribute value ....`.

[query_analyser.py](./query_analyser.py): takes as an argument a query, analyses its predicates and propagates the query request to the controller
with appropriate arguments to perform the necessary checks.

[helper.py](./helper.py): contains generic helper functions for the policy compiler