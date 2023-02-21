import sys
import json
from policy_compiler.helper import safe_open
from policy_compiler.KV_interface import rewrite_query
import argparse

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
    "objObjectionsIs",
    "objOwner",
    "objOwnerIs",
    "monitor",
    "query"
]

def has_not(pred_attr):
    if pred_attr[0] == '!':
        return (pred_attr[1:], '!')
    else:
        return (pred_attr, '')

def analyze_query(query):
    '''
    Policy predicates are separated by '&'.
    The '|' relationship is specified in the predicates through 
    multiple specified values.
    e.g. sessionKeyIs("auth_user_pub_key", "auth_user2_pub_key", ...)
    Hence, it makes sense to split them based on "&" symbol.
    '''
    query = query.strip().split("&")
    query_cmd = "" # variable for the actual query requested
    filter = "" # variable with the predicates parameter for the controller
    for pred in query:
        pred_attr = pred.split('(')[0]
        (pred_attr, negated) = has_not(pred_attr)
        if pred_attr not in policy_predicates:
            print(f"We do not support policy predicate: {pred_attr}")
            sys.exit(1)
            
        pred_val = pred.split(pred_attr)[1][1:-1]
        if (pred_attr == "query"):
            query_cmd = pred_val
        else:
            pred_val = pred_val[1:-1] # "val" -> val for parsing 
            filter += (f" -{pred_attr} {negated}{pred_val}") 
    
    # print(query_cmd, filter)
    return rewrite_query(query_cmd, filter)

def main():
    parser = argparse.ArgumentParser(description='Analyse a GDPRuler query workload trace.')
    parser.add_argument('-f', '--file', help='path to the workload trace file', 
                        dest='path', default=None, nargs=1, required=True, type=str)
    args = parser.parse_args(sys.argv[1:]) # to exclude the script name

    queries_file = args.path[0] # the file containing the queries to test
    queries_file = safe_open(queries_file, "r")
    queries = queries_file.readlines()
    
    for query in queries:
        print(analyze_query(query.rstrip()))
    
    return

if __name__ == "__main__":
    main()