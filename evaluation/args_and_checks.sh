#!/bin/sh

set -e

# Get the directory where the script resides
script_dir="$(dirname "$(readlink -f "$0")")"

# Find the root directory of the repository
project_root=$(git rev-parse --show-toplevel 2>/dev/null)

# Function to parse command-line arguments and perform checks
parse_args_and_checks() {
  # Set default encryption value
  encr="OFF"
  # Check if the script is called with a parameter
  if [ $# -gt 0 ]; then
    # Check the parameter value
    case "$1" in
      --encr)
        # Check if the next argument is provided
        if [ $# -gt 1 ]; then
          case "$2" in
            ON)
              encr="ON"
              ;;
            OFF)
              encr="OFF"
              ;;
            *)
              echo "Invalid value for --encr. Please use 'ON' or 'OFF'."
              exit 1
              ;;
          esac
        else
            echo "Value for --encr is missing. Please provide 'ON' or 'OFF'."
            exit 1
        fi
        ;;
      *)
        echo "Invalid option. Usage: $0 --encr [ON|OFF]"
        exit 1
        ;;
    esac
  fi

  # Build the controller
  echo "Building controller with encryption set to $encr"
  pushd ${project_root}/controller
  cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=$encr;
  cmake --build build -j$(nproc)
  popd
}