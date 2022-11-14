import sys
import json
from helper import safe_open
from KV_interface import *

policy_predicates = [
    "eq",
    "le",
    "lt",
    "ge",
    "gt",
    "sessionKey",
    "sessionKeyIs",
    "objExp",
    "objExpIs",
    "objPur",
    "objPurIs",
    "objOrig",
    "objOrigIs",
    "objShare",
    "objShareIs",
    "objObjections",
    "objObjectionsIs"
    "objOwner",
    "objOwnerIs",
    "monitor",
    "query"
]

def analyze_query(query):
    '''
    Policy predicates are separated by '&'.
    The '|' relationship is specified in the predicates through multiple specified values.
    e.g. sessionKeyIs("auth_user_pub_key", "auth_user2_pub_key", ...)
    Hence, it makes sense to split them based on "&" symbol.
    '''
    query = query.rstrip().split("&")
    print(query)
    for pred in query:
        pred_type = pred.split('(')[0]
        print(pred_type)
    return

def main():
    queries_file = sys.argv[1] # the file containing the queries to test
    queries_file = safe_open(queries_file, "r")
    queries = queries_file.readlines()
    
    for query in queries:
        analyze_query(query.rstrip())

if __name__ == "__main__":
    main()