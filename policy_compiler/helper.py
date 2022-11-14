import sys

def safe_open(path, perms):
    try:
        file = open(path, perms)
    except OSError:
        print (f"Could not open file: {path}")
        sys.exit()
    return file