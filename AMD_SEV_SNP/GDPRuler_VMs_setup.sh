#!/bin/bash

set -e

# Script to setup up a server & client VM for GDPRuler

THIS_DIR=$(dirname "$(readlink -f "$0")")

# Preparation Steps and checks
if ! command -v "cloud-localds" ; then
  echo "cloud-localds not found, please install it"
  exit 1
fi

QEMU_DIR=${THIS_DIR}/AMDSEV/qemu
QEMU_IMG_BIN=${THIS_DIR}/AMDSEV/usr/local/bin/qemu-img
OVMF_DIR=${THIS_DIR}/AMDSEV/ovmf
OVMF_CODE=${THIS_DIR}/AMDSEV/usr/local/share/qemu/OVMF_CODE.fd
OVMF_VARS=${THIS_DIR}/AMDSEV/usr/local/share/qemu/OVMF_VARS.fd

if [[ ! -d ${QEMU_DIR} ]] ; then
  echo "${QEMU_DIR} does not exist. Please build it by running \"bash ./build.sh qemu\" in the AMDSEV directory"
  exit 1
fi

if [[ ! -f ${QEMU_IMG_BIN} ]] ; then
  echo "${QEMU_IMG_BIN} does not exist. Please build it by running \"bash ./build.sh qemu\" in the AMDSEV directory"
  exit 1
fi

if [[ ! -d ${OVMF_DIR} ]] ; then
  echo "${OVMF_DIR} does not exist. Please build it by running \"bash ./build.sh ovmf\" in the AMDSEV directory"
  exit 1
fi

if [[ ! -f ${OVMF_CODE} ]] ; then
  echo "${OVMF_CODE} does not exist. Please build it by running \"bash ./build.sh ovmf\" in the AMDSEV directory"
  exit 1
fi

if [[ ! -f ${OVMF_VARS} ]] ; then
  echo "${OVMF_VARS} does not exist. Please build it by running \"bash ./build.sh ovmf\" in the AMDSEV directory"
  exit 1
fi

# Retrieve the initial image
IMG_URL=https://cloud-images.ubuntu.com/jammy/current
CLOUD_IMG=jammy-server-cloudimg-amd64.img
if [[ ! -f ${CLOUD_IMG} ]] ; then
  wget ${IMG_URL}/${CLOUD_IMG}
fi

# Create the appropriate directories
mkdir -p images
mkdir -p OVMF_files

# Set-up the controller VM
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ${QEMU_IMG_BIN} convert jammy-server-cloudimg-amd64.img ./images/controller.img
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH  ${QEMU_IMG_BIN} resize ./images/controller.img +30G
bash prepare_net_cfg.sh -br virbr0 -cfg ./cloud_configs/network-config-controller.yml
sudo cloud-localds -N ./cloud_configs/network-config-controller.yml ./images/controller-cloud-config.iso ./cloud_configs/cloud-config-controller
mkdir -p OVMF_files/controller
cp ${OVMF_CODE} ./OVMF_files/controller/OVMF_CODE.fd
cp ${OVMF_VARS} ./OVMF_files/controller/OVMF_VARS.fd

# Set-up the server VM
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ${QEMU_IMG_BIN} convert jammy-server-cloudimg-amd64.img ./images/server.img
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH  ${QEMU_IMG_BIN} resize ./images/server.img +30G
bash prepare_net_cfg.sh -br virbr0 -cfg ./cloud_configs/network-config-server.yml
sudo cloud-localds -N ./cloud_configs/network-config-server.yml ./images/server-cloud-config.iso ./cloud_configs/cloud-config-server
mkdir -p OVMF_files/server
cp ${OVMF_CODE} ./OVMF_files/server/OVMF_CODE.fd
cp ${OVMF_VARS} ./OVMF_files/server/OVMF_VARS.fd

# Set-up the client VM
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ${QEMU_IMG_BIN} convert jammy-server-cloudimg-amd64.img ./images/client.img
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH  ${QEMU_IMG_BIN} resize ./images/client.img +30G
bash prepare_net_cfg.sh -br virbr0 -cfg ./cloud_configs/network-config-client.yml
sudo cloud-localds -N ./cloud_configs/network-config-client.yml ./images/client-cloud-config.iso ./cloud_configs/cloud-config-client
mkdir -p OVMF_files/client
cp ${OVMF_CODE} ./OVMF_files/client/OVMF_CODE.fd
cp ${OVMF_VARS} ./OVMF_files/client/OVMF_VARS.fd
