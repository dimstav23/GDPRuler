import sys
import json
from policy_compiler.helper import safe_open
import argparse

mandatory_keys = ["sessionKey", "default_policy"]

def format_args(attr, value):
    return " -{} {}".format(attr, value)

def setup_user(user_policy):
    setup_args = "user_policy"   
    for config in user_policy.keys():
        # get the general purpose configs
        if (config != "default_policy"):
            setup_args += format_args(config, user_policy[config])
        else:
            # get the nested default policies of the user
            for def_policy in user_policy[config].keys():
                setup_args += format_args(def_policy, ','.join(user_policy[config][def_policy]))

    return setup_args

# checks if the mandatory keys are present in the list
def check_config(user_policy):
    if not(set(user_policy.keys()).issuperset(mandatory_keys)):
        print("Mandatory keys are not present in the config")
        return False
    return True

# parse user policy and return the setup
def parse_user_policy(user_policy):
  if (check_config(user_policy)):
    return setup_user(user_policy)
  else:
    return None

def main():   
  parser = argparse.ArgumentParser(description='Process a default policy configuration.')
  parser.add_argument('-f', '--file', help='path to the configuration file', 
                        dest='path', default=None, nargs=1, required=True, type=str)
  args = parser.parse_args(sys.argv[1:]) # to exclude the script name
    
  user_policy = args.path[0] # the file containing the user config
  user_policy = safe_open(user_policy, "r")
  user_policy = json.load(user_policy)

  print(parse_user_policy(user_policy))
  return

if __name__ == "__main__":
    main()