import sys
import json
from helper import safe_open

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

def main():
    user_policy = sys.argv[1] # the file containing the user config
    user_policy = safe_open(user_policy, "r")
    user_policy = json.load(user_policy)

    if (check_config(user_policy)):
        # print(setup_user(user_policy))
        return setup_user(user_policy)
    else:
        return None

if __name__ == "__main__":
    main()