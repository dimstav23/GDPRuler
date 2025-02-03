#!/bin/sh

set -e

# Script to setup up a server & client VM for GDPRuler

THIS_DIR=$(dirname "$(readlink -f "$0")")

# Preparation Steps and checks

# check for cloud-localds - required for cloud-init
CLOUD_LOCALDS_BIN=$(command -v cloud-localds)
if [[ -z "$CLOUD_LOCALDS_BIN" ]]; then
  echo "cloud-localds not found, please install it" >&2
  exit 1
fi

# check for qemu version
QEMU_BIN=$(command -v qemu-system-x86_64)
QEMU_IMG_BIN=$(command -v qemu-img)
MIN_QEMU_VERSION="9.2.0"

if [[ -z "$QEMU_BIN" ]]; then
  echo "QEMU not installed." >&2
  exit 1
fi

QEMU_VERSION=$("$QEMU_BIN" --version 2>/dev/null | grep -oP 'version \K\d+\.\d+\.\d+')
if [[ -z "$QEMU_VERSION" ]] || [[ $(echo -e "$QEMU_VERSION\n9.2.0" | sort -V | head -n1) != "9.2.0" ]]; then
    echo "QEMU version $QEMU_VERSION is < 9.2.0 or not found."
    exit 1
fi

# check for ovmf
OVMF_DIR=${THIS_DIR}/AMDSEV/ovmf
OVMF=${THIS_DIR}/AMDSEV/usr/local/share/qemu/OVMF.fd
OVMF_FILES_DIR=${THIS_DIR}/firmware

if [[ ! -d ${OVMF_DIR} ]] ; then
  echo "${OVMF_DIR} does not exist. Please build it by running \"bash ./build.sh ovmf\" in the AMDSEV directory"
  exit 1
fi
if [[ ! -f ${OVMF} ]] ; then
  echo "${OVMF} does not exist. Please build it by running \"bash ./build.sh ovmf\" in the AMDSEV directory"
  exit 1
fi

# check for the existence of the virtual bridge for networking
BRIDGE_NAME=virbr0
BRIDGE_IP=192.168.122.1
NETMASK=255.255.255.0

# Check for brctl
if ! command -v brctl &> /dev/null; then
  echo "Error: brctl not found. Install bridge-utils first."
  exit 1
fi

# Remove existing bridge if present
if [ -d "/sys/class/net/$BRIDGE_NAME" ]; then
  echo "Removing existing $BRIDGE_NAME..."
  sudo ip link set dev $BRIDGE_NAME down
  sudo brctl delbr $BRIDGE_NAME || exit 1
fi

# Create new bridge
echo "Creating new $BRIDGE_NAME..."
sudo brctl addbr $BRIDGE_NAME || exit 1
sudo brctl stp $BRIDGE_NAME on || exit 1

# Configure bridge IP
sudo ip link set dev $BRIDGE_NAME up || exit 1
sudo ip addr add $BRIDGE_IP/$NETMASK dev $BRIDGE_NAME || exit 1

echo "Bridge $BRIDGE_NAME reconfigured:"
ip addr show $BRIDGE_NAME

# Setup the VMs
IMAGES_DIR=${THIS_DIR}/images
# Create the appropriate directories for the images and OVMF files
mkdir -p ${IMAGES_DIR}
mkdir -p ${OVMF_FILES_DIR}

# Retrieve the initial image
IMG_URL=https://cloud-images.ubuntu.com/noble/current
CLOUD_IMG=noble-server-cloudimg-amd64.img
if [[ ! -f ${THIS_DIR}/${CLOUD_IMG} ]] ; then
  wget ${IMG_URL}/${CLOUD_IMG}
fi

# Set-up the controller VM
echo "[1/7] Setting up the controller VM files"
LD_LIBRARY_PATH=$LD_LIBRARY_PATH ${QEMU_IMG_BIN} convert ${THIS_DIR}/${CLOUD_IMG} ${IMAGES_DIR}/controller.img
LD_LIBRARY_PATH=$LD_LIBRARY_PATH ${QEMU_IMG_BIN} resize ${IMAGES_DIR}/controller.img +20G
mkdir -p ${OVMF_FILES_DIR}/controller
cp ${OVMF} ${OVMF_FILES_DIR}/controller/OVMF.fd
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
cp ${OVMF} ${OVMF_FILES_DIR}/server/OVMF.fd
cp ${IMAGES_DIR}/controller.img ${IMAGES_DIR}/server.img

# Set-up the client VM
echo "[4/7] Setting up the client VM files"
mkdir -p ${OVMF_FILES_DIR}/client
cp ${OVMF} ${OVMF_FILES_DIR}/client/OVMF.fd
cp ${IMAGES_DIR}/controller.img ${IMAGES_DIR}/client.img

# Import the network config for the controller in the VM image
echo "[5/7] Setting up the controller network configuration"
bash ${THIS_DIR}/prepare_net_cfg.sh -br ${BRIDGE_NAME} -cfg ${THIS_DIR}/network_configs/netplan-controller.yaml
virt-customize --add ${IMAGES_DIR}/controller.img \
  --copy-in ${THIS_DIR}/network_configs/netplan-controller.yaml:/etc/netplan/

# Import the network config for the server in the VM image
echo "[6/7] Setting up the server network configuration"
bash ${THIS_DIR}/prepare_net_cfg.sh -br ${BRIDGE_NAME} -cfg ${THIS_DIR}/network_configs/netplan-server.yaml
virt-customize --add ${IMAGES_DIR}/server.img \
  --copy-in ${THIS_DIR}/network_configs/netplan-server.yaml:/etc/netplan/

# Import the network config for the client in the VM image
echo "[7/7] Setting up the client network configuration"
bash ${THIS_DIR}/prepare_net_cfg.sh -br ${BRIDGE_NAME} -cfg ${THIS_DIR}/network_configs/netplan-client.yaml
virt-customize --add ${IMAGES_DIR}/client.img \
  --copy-in ${THIS_DIR}/network_configs/netplan-client.yaml:/etc/netplan/
