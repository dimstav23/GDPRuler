#!/bin/bash

# Get the directory where the script resides
script_dir="$(dirname "$(readlink -f "$0")")"

# Set default values
configs_folder="$script_dir/configs"
purposes=64
clients=64
user_id=0

# Function to display usage instructions
usage() {
    echo "Usage: $0 [-pur <value>] [-clients <value>] [-user_id <value>]"
    echo "Options:"
    echo "  -pur          : Number of purposes (default: $purposes)"
    echo "  -clients      : Total number of clients (default: $clients)"
    echo "  -uid          : User ID (default: $user_id)"
    exit 1
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -pur)
            purposes="$2"
            shift
            shift
            ;;
        -clients)
            clients="$2"
            shift
            shift
            ;;
        -uid)
            user_id="$2"
            shift
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Invalid option: $key"
            usage
            ;;
    esac
done

allowed_pur=$((purposes / 2))

# Generate the expanded "purpose" array
purpose=""
for ((i=0; i<allowed_pur; i++))
do
    purpose+="      \"purpose$i\""
    if [ $i -ne $((allowed_pur-1)) ]; then
        purpose+=","
    fi
    purpose+=$'\n'
done

# Generate the expanded "objection" array
objection=""
for ((i=allowed_pur; i<purposes; i++))
do
    objection+="      \"purpose$i\""
    if [ $i -ne $((purposes-1)) ]; then
        objection+=","
    fi
    objection+=$'\n'
done

# Generate the expanded "objShare" array
objShare=""
for ((i=0; i<clients; i++))
do
    if [ $i -ne $user_id ]; then
        objShare+="      \"user$i\""
        if [ $i -ne $((clients-1)) ]; then
            objShare+=","
        fi
        objShare+=$'\n'
    fi
done

# Construct the JSON object
policy_json=$(cat <<EOF
{
  "sessionKey": "user${user_id}",
  "encryption": "true",
  "default_policy": {
    "purpose": [
$purpose    ],
    "objection": [
$objection    ],
    "origin": [
      "src0"
    ],
    "expTime": [
      "0"
    ],
    "objShare": [
$objShare    ],
    "monitor": [
      "false"
    ]
  }
}
EOF
)

# create the folder for the client default policies
mkdir -p ${configs_folder}

echo "$policy_json" > ${configs_folder}/client${user_id}_config.json
