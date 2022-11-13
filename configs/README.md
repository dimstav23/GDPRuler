## GDPR compliant queries examples

### #5 Purpose limitation: Collect data for explicit purposes
The user that creates the data specifies the desired policy properties. He/She also has [default policies](./owner_policy.json)
that are applied for the missing values. For 3rd party services to operate on the keys, they should have access on them
which is defined by the user through the `objShare` property and they also have to state the purpose of their query, otherwise
a [default purpose](./3p_policy.json) is applied based on their generic configuration file.
```
- query(put("2", "VALUE_2"))&objPur("analytics") & objOrig("mysite.com") & objShare("3p_pub_key_0") & objObjections("advertising")
- query(get("2")) & sessionKeyIs("3p_pub_key") & objPurIs("analytics")
```

### #5 Storage limitation: Do not store data indefinitely
The user/3p-service can explicitly specify to filter out expired entrier.
However, for generality, if the user specifies the expire time placeholder in the configuration 
with the value `check`, the expiration check is applied implicitly on every query. 
For performance reasons, the background operation for deleting stale entries runs at a configurable interval.
An example of such a config is shown [here](./owner_policy.json).
```
- query(get("2")) & sessionKeyIs("owner_pub_key") & le(time(), objExpT("2"))
- query(delete("__GDPRuler_data")) & sessionKeyIs("owner_pub_key") & gt(time(), objExpIs("2015-05-16T05:50:06"))
```

### #13, #14 Information to be provided: provide GDPR metadata
The owner or an authorized user can use the `getm` query operation to retrieve the GDPR metadata 
either related to a specific key or based on a set of predicates.
```
- query(getm(k)) & sessionKeyIs("owner_pub_key")
- query(getm("__GDPRuler_metadata")) & sessionKeyIs("owner_pub_key") & objPurIs("analytics") & ...
```

### #15 Right of access by users: users access all their data
A user can request and retrieve all his data with the use of the `"__GDPRuler_data"` placeholder in a get query.
Additionally, one can update the KV pairs normally provided there are access privileges.
```
- query(get("2")) & sessionKeyIs("owner_pub_key")
- query(get("__GDPRuler_data")) & sessionKeyIs("owner_pub_key")
- query(put("2",  "VALUE_2")) & sessionKeyIs("owner_pub_key")
```

### #17 Right to be forgotten: Allow customers to erase their data
Users can request the deletion of all their data.
```
- query(delete("__GDPRuler_data")) & sessionKeyIs("owner_pub_key")
```

### #21 Right to object: Do not use data for objected reasons
The list of objections is examined in every query for data that have to be used by a 3rd party 
for a specific purpose. A 3rd-party can also explicitly filter out the data that disallow 
processing for a specific purpose.
```
- query(get("2")) & sessionKeyIs("3p_pub_key") & objPurIs("analytics") & !objObjectionsIs("analytics")
- query(get("__GDPRuler_data")) & sessionKeyIs("3p_pub_key") & objPurIs("analytics") & !objObjectionsIs("analytics")
```

### #22 Automated individual decision-making
Can be deducted through #5 and #21 predicates.

### #25 Data protection by design and default - access control
Data is accessible only from users with the required access rights.
They are declared through the `objShare` property of the policy of each key defined by the data owner.
```
- query(get("__GDPRuler_data")) & sessionKeyIs("auth_user_pub_key") | sessionKeyIs("auth_user2_pub_key") | ...
- query(put("2", "VALUE_2")) & sessionKeyIs("auth_user_pub_key") | sessionKeyIs("auth_user2_pub_key") | ...
- query(delete("__GDPRuler_data")) & sessionKeyIs("auth_user_pub_key") | sessionKeyIs("auth_user2_pub_key") | ...
```

### #28 Processor: Do not grant unlimited access to data
Only authorized users can access the data.
```
- query(get("__GDPRuler_data")) & sessionKeyIs("auth_user_pub_key") | sessionKeyIs("auth_user2_pub_key") | ...
- query(put("2", "VALUE_2")) & sessionKeyIs("auth_user_pub_key") | sessionKeyIs("auth_user2_pub_key") | ...
- query(delete("__GDPRuler_data")) & sessionKeyIs("auth_user_pub_key") | sessionKeyIs("auth_user2_pub_key") | ...
```

### #30 Records of processing activity
Instruct GDPRuler to monitor a value (if the setting is not the default):
```
- query(putm("1")) & sessionKeyIs("owner_pub_key") & monitor("true")
```
```
- query(getLogs("__regulator_log")) & sessionKeyIs("owner_pub_key") & objOwnerIs("owner_pub_key")
```

### #32 Security of processing
- Encryption of the data, metadata and logs
- Secure execution environment (e.g., SEV)

### #33 Personal data breach: Share logs from affected systems
Get the logs for a unique key or based on a set of predicates. 
Here is an example for the log of a single key and a specified user's data:
```
- query(getLogs("1")) & sessionKeyIs("regulator_key")
- query(getLogs("__regulator_log")) & sessionKeyIs("regulator_key") & objOwnerIs("owner_pub_key")
```


#### TODO:
- Add parameter for metadata update to choose if a user wants to replace or append options. Currently 
only append is supported. 