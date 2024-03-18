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

IMAGES_DIR=/scratch/dimitrios/images
OVMF_FILES_DIR=/scratch/dimitrios/OVMF_files

VIRTUAL_BRIDGE=virbr0

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
if [[ ! -f ${THIS_DIR}/${CLOUD_IMG} ]] ; then
  wget ${IMG_URL}/${CLOUD_IMG}
fi

# Create the appropriate directories for the images and OVMF files
mkdir -p ${IMAGES_DIR}
mkdir -p ${OVMF_FILES_DIR}

# Set-up the controller VM
echo "[1/7] Setting up the controller VM files"
LD_LIBRARY_PATH=$LD_LIBRARY_PATH ${QEMU_IMG_BIN} convert ${THIS_DIR}/${CLOUD_IMG} ${IMAGES_DIR}/controller.img
LD_LIBRARY_PATH=$LD_LIBRARY_PATH  ${QEMU_IMG_BIN} resize ${IMAGES_DIR}/controller.img +30G
mkdir -p ${OVMF_FILES_DIR}/controller
cp ${OVMF_CODE} ${OVMF_FILES_DIR}/controller/OVMF_CODE.fd
cp ${OVMF_VARS} ${OVMF_FILES_DIR}/controller/OVMF_VARS.fd
echo "[2/7] Installing software in the controller VM image -- to be used as a base"
virt-customize --add ${IMAGES_DIR}/controller.img \
  --root-password password:123456 \
  --edit '/etc/ssh/sshd_config:s/#PermitRootLogin prohibit-password/PermitRootLogin yes/' \
  --edit '/etc/ssh/sshd_config:s/PasswordAuthentication no/PasswordAuthentication yes/' \
  --run-command 'growpart /dev/sda 1' \
  --run-command 'resize2fs /dev/sda1' \
  --run-command 'ssh-keygen -A' \
  --run-command 'systemctl mask pollinate.service' \
  --copy-in ${THIS_DIR}/setup_vm.sh:/root \
  --smp $(nproc) \
  --memsize 16384 \
  --run-command /root/setup_vm.sh

# Set-up the server VM
echo "[3/7] Setting up the server VM files"
mkdir -p ${OVMF_FILES_DIR}/server
cp ${OVMF_CODE} ${OVMF_FILES_DIR}/server/OVMF_CODE.fd
cp ${OVMF_VARS} ${OVMF_FILES_DIR}/server/OVMF_VARS.fd
cp ${IMAGES_DIR}/controller.img ${IMAGES_DIR}/server.img

# Set-up the client VM
echo "[4/7] Setting up the client VM files"
mkdir -p ${OVMF_FILES_DIR}/client
cp ${OVMF_CODE} ${OVMF_FILES_DIR}/client/OVMF_CODE.fd
cp ${OVMF_VARS} ${OVMF_FILES_DIR}/client/OVMF_VARS.fd
cp ${IMAGES_DIR}/controller.img ${IMAGES_DIR}/client.img


# Import the network config for the controller in the VM image
echo "[5/7] Setting up the controller network configuration"
bash ${THIS_DIR}/prepare_net_cfg.sh -br ${VIRTUAL_BRIDGE} -cfg ${THIS_DIR}/network_configs/netplan-controller.yaml
virt-customize --add ${IMAGES_DIR}/controller.img \
  --copy-in ${THIS_DIR}/network_configs/netplan-controller.yaml:/etc/netplan/

# Import the network config for the server in the VM image
echo "[6/7] Setting up the server network configuration"
bash ${THIS_DIR}/prepare_net_cfg.sh -br ${VIRTUAL_BRIDGE} -cfg ${THIS_DIR}/network_configs/netplan-server.yaml
virt-customize --add ${IMAGES_DIR}/server.img \
  --copy-in ${THIS_DIR}/network_configs/netplan-server.yaml:/etc/netplan/

# Import the network config for the client in the VM image
echo "[7/7] Setting up the client network configuration"
bash ${THIS_DIR}/prepare_net_cfg.sh -br ${VIRTUAL_BRIDGE} -cfg ${THIS_DIR}/network_configs/netplan-client.yaml
virt-customize --add ${IMAGES_DIR}/client.img \
  --copy-in ${THIS_DIR}/network_configs/netplan-client.yaml:/etc/netplan/
