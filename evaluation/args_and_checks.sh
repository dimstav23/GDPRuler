#!/bin/sh

set -e

# Get the directory where the script resides
script_dir="$(dirname "$(readlink -f "$0")")"

# Find the root directory of the repository
project_root=$(git rev-parse --show-toplevel 2>/dev/null)

# Function to parse command-line arguments and perform checks
parse_args_and_checks() {
  # Set default encryption value
  encryption="OFF"
  logging="OFF"
  # Parse the arguments
  while [ $# -gt 0 ]; do
    case "$1" in
      --encryption)
        # Check if the next argument is provided
        if [ $# -gt 1 ]; then
          case "$2" in
            ON)
              encryption="ON"
              shift
              ;;
            OFF)
              encryption="OFF"
              shift
              ;;
            *)
              echo "Invalid value for --encryption. Please use 'ON' or 'OFF'."
              exit 1
              ;;
          esac
        else
            echo "Value for --encryption is missing. Please provide 'ON' or 'OFF'."
            exit 1
        fi
        ;;
      --logging)
        # Check if the next argument is provided
        if [ $# -gt 1 ]; then
          case "$2" in
            ON)
              logging="ON"
              shift
              ;;
            OFF)
              logging="OFF"
              shift
              ;;
            *)
              echo "Invalid value for --logging. Please use 'ON' or 'OFF'."
              exit 1
              ;;
          esac
        else
            echo "Value for --logging is missing. Please provide 'ON' or 'OFF'."
            exit 1
        fi
        ;;
      *)
        echo "Invalid option: $1. Usage: $0 --encryption [ON|OFF] --logging [ON|OFF]"
        exit 1
        ;;
    esac
    shift
  done

  # Build the controller
  echo "Building controller with encryption set to $encryption"
  pushd ${project_root}/controller
  cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=$encryption;
  cmake --build build -j$(nproc)
  popd
}