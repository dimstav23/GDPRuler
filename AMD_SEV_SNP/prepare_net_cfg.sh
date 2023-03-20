#!/bin/bash

usage() {
	echo "$0 [options]"
	echo "Mandatory arguments:"
	echo " -br            virtual bridge name"
	echo " -cfg           network config file path"
	exit 1
}

while [ -n "$1" ]; do
	case "$1" in
		-br)    BRIDGE=$2
			shift
			;;
		-cfg)   NETCFG=$2
			shift
			;;
		
		*)  usage;;
	esac
	shift
done

if [ -z "$BRIDGE" ] || [ -z "$NETCFG" ]; then
    usage
fi

# Get the IP prefix for the virtual bridge network
PREFIX=$(ip address show dev "${BRIDGE}" | grep inet | awk '{print $2}' | awk -F . '{print $1"."$2"."$3}')

# Replace the first part of the address and gateway in the config .yml file
# Note that the last part of the address and gateway will remain the same as the one pre-defined
sed -i -E "s/^(\s+)(address:\s+)[0-9]+\.[0-9]+\.[0-9]+\./\1\2${PREFIX}./" "${NETCFG}"
sed -i -E "s/^(\s+)(gateway:\s+)[0-9]+\.[0-9]+\.[0-9]+\./\1\2${PREFIX}./" "${NETCFG}"

