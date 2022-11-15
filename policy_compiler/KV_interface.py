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
    
def execute_query(query, metadata):
    return query_multiplexer(query, metadata)

def query_multiplexer(query, metadata):
    query_split = query.split('(')
    query_cmd = query_split[0].lower().strip()
    query_args = query_split[1].strip()[:-1]
    K = getKey(query_args)
    
    if query_cmd == "put":
        V = getValue(query_args)
        put_filtered(K, V, metadata) 
    elif query_cmd == "get":
        get_filtered(K, metadata)
    elif query_cmd == "delete":
        delete_filtered(K, metadata)
    elif query_cmd == "putm":
        putm(K, metadata) 
    elif query_cmd == "getm":
        getm(K, metadata)
    elif query_cmd == "deletem":
        deletem(K, metadata)
    elif query_cmd == "getlogs":
        getLogs(K, metadata)
    else:
        print(f"Query type {query_cmd} not supported")
        exit(1)
    return True

# intact KV API
def put(K, V):
    print(f"put {K} {V}")
    return

def get(K):
    print(f"get {K}")
    return

def delete(K):
    print(f"delete {K}")
    return

# GDPR-enhanced API
def put_filtered(K, V, metadata):
    if metadata == "":
        return put(K, V)
    
    print(f"put_filtered {K} {V} {metadata}")
    return

def get_filtered(K, metadata):
    if metadata == "":
        return get(K)
    
    print(f"get_filtered {K} {metadata}")
    return

def delete_filtered(K, metadata):
    if metadata == "":
        return delete(K)
    
    print(f"delete_filtered {K} {metadata}")
    return

# GDPR-metadata API
def putm(K, metadata):
    print(f"putm {K} {metadata}")
    return

def getm(K, metadata):
    print(f"getm {K} {metadata}")
    return

def deletem(K, metadata):
    print(f"deletem {K} {metadata}")
    return

# Regulator API
def getLogs(K, metadata):
    print(f"getLogs {K} {metadata}")
    return