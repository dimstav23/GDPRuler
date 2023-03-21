#!/bin/bash
# SPDX-License-Identifier: MIT

SCRIPT_DIR="$(dirname $0)"
. ${SCRIPT_DIR}/common.sh
. ${SCRIPT_DIR}/stable-commits
[ -e /etc/os-release ] && . /etc/os-release

function usage()
{
	echo "Usage: $0 [OPTIONS] [COMPONENT]"
	echo "  where COMPONENT is an individual component to build:"
	echo "    qemu, ovmf, kernel [host|guest]"
	echo "    (default is to build all components)"
	echo "  where OPTIONS are:"
	echo "  --install PATH          Installation path (default $INSTALL_DIR)"
	echo "  -h|--help               Usage information"

	exit 1
}

INSTALL_DIR="`pwd`/usr/local"

while [ -n "$1" ]; do
	case "$1" in
	--install)
		[ -z "$2" ] && usage
		INSTALL_DIR="$2"
		shift; shift
		;;
	-h|--help)
		usage
		;;
	-*|--*)
		echo "Unsupported option: [$1]"
		usage
		;;
	*)
		break
		;;
	esac
done

mkdir -p $INSTALL_DIR
IDIR=$INSTALL_DIR
INSTALL_DIR=$(readlink -e $INSTALL_DIR)
[ -n "$INSTALL_DIR" -a -d "$INSTALL_DIR" ] || {
	echo "Installation directory [$IDIR] does not exist, exiting"
	exit 1
}

if [ -z "$1" ]; then
	build_install_qemu "$INSTALL_DIR"
	build_install_ovmf "$INSTALL_DIR/share/qemu"
else
	case "$1" in
	qemu)
		build_install_qemu "$INSTALL_DIR"
		;;
	ovmf)
		build_install_ovmf "$INSTALL_DIR/share/qemu"
		;;
	esac
fi
