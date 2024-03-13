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
# Note: '-w' is added in grep to only get the IPv4 and not the IPv6 address
PREFIX=$(ip address show dev "${BRIDGE}" | grep -w inet | awk '{print $2}' | awk -F . '{print $1"."$2"."$3}')

if [[ -z "$PREFIX" ]]; then
    echo "Network prefix not found -- please make sure that the bridge "${BRIDGE}" exists."
    exit 1
fi

# Replace the first part of the address and gateway in the config .yml file
# Note that the last part of the address and gateway will remain the same as the one pre-defined
sed -i -E "s/^(\s+)(addresses:\s\[+)[0-9]+\.[0-9]+\.[0-9]+\./\1\2${PREFIX}./" "${NETCFG}"
sed -i -E "s/^(\s+)(gateway4:\s+)[0-9]+\.[0-9]+\.[0-9]+\./\1\2${PREFIX}./" "${NETCFG}"
