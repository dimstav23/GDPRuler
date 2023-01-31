query_types = [
    "put",
    "get",
    "delete",
    "putm",
    "getm",
    "deletem",
    "getlogs"
]

def getValue(query_args):
    try:
        return query_args.split(',')[1].strip()
    except IndexError:
        print("No value specified for put query.")
        exit(1)
    
def getKey(query_args):
    try:
        return query_args.split(',')[0].strip()
    except IndexError:
        print("No key specified for requested query.")
        exit(1)
    
def rewrite_query(query, metadata):
    return query_multiplexer(query, metadata)

def query_multiplexer(query, metadata):
    query_split = query.split('(')
    query_cmd = query_split[0].lower().strip()
    query_args = query_split[1].strip()[:-1]
    K = getKey(query_args)
    
    if query_cmd == "put":
        V = getValue(query_args)
        return put_filtered(K, V, metadata) 
    elif query_cmd == "get":
        return get_filtered(K, metadata)
    elif query_cmd == "delete":
        return delete_filtered(K, metadata)
    elif query_cmd == "putm":
        return putm(K, metadata) 
    elif query_cmd == "getm":
        return getm(K, metadata)
    elif query_cmd == "deletem":
        return deletem(K, metadata)
    elif query_cmd == "getlogs":
        return getLogs(K, metadata)
    else:
        print(f"Query type {query_cmd} not supported")
        exit(1)

# intact KV API
def put(K, V):
    return (f"put {K} {V}")

def get(K):
    return (f"get {K}")

def delete(K):
    return (f"delete {K}")

# GDPR-enhanced API
def put_filtered(K, V, metadata):
    if metadata == "":
        return put(K, V)
    
    return (f"put {K} {V} {metadata}")

def get_filtered(K, metadata):
    if metadata == "":
        return get(K)
    
    return (f"get {K} {metadata}")

def delete_filtered(K, metadata):
    if metadata == "":
        return delete(K)
    
    return (f"delete {K} {metadata}")

# GDPR-metadata API
def putm(K, metadata):
    return (f"putm {K} {metadata}")

def getm(K, metadata):
    return (f"getm {K} {metadata}")

def deletem(K, metadata):
    return (f"deletem {K} {metadata}")

# Regulator API
def getLogs(K, metadata):
    return (f"getLogs {K} {metadata}")