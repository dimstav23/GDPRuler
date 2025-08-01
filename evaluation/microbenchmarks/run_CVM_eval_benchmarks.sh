#!/bin/bash
set -euxo pipefail

## Paths
IMG_URL="https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img"
BASE_IMG="noble-server-cloudimg-amd64.img"
VM_IMG="my-ubuntu-vm.img"
CVM_IMG_DIR="CVM_eval/gdpr_setup/images"
CVM_OVMF_DIR="CVM_eval/gdpr_setup/ovmf"
CVM_IMG="$CVM_IMG_DIR/gdpr.img"
SSH_KEY_PATH="CVM_eval/nix/ssh_key.pub"
FIRMWARE_SRC="../../CVM_setup/firmware/gdpr"
FIRMWARE_DEST="$CVM_OVMF_DIR"
NETPLAN_SRC="../../CVM_setup/network_configs/netplan-gdpr-cvm-eval.yaml"
NETPLAN_DEST="CVM_eval/gdpr_setup/netplan-gdpr-cvm-eval.yaml"

## Step 1: Setup directories
mkdir -p "$CVM_IMG_DIR" "$CVM_OVMF_DIR"

## Step 2: Download base image if missing
if [ ! -f "$BASE_IMG" ]; then
  wget --no-clobber "$IMG_URL"
fi

## Step 3: Convert .img to QCOW2/raw if not already done
if [ ! -f "$VM_IMG" ]; then
  qemu-img convert "$BASE_IMG" "$VM_IMG"
fi

## Step 4: Prepare project image if missing
if [ ! -f "$CVM_IMG" ]; then
  cp "$VM_IMG" "$CVM_IMG"
  qemu-img resize "$CVM_IMG" +10G
fi

## Step 5: Copy firmware (skip if up to date)
if [ ! -d "$FIRMWARE_SRC" ]; then
  echo "FIRMWARE source directory missing: $FIRMWARE_SRC"
  exit 1
fi
cp -ru "$FIRMWARE_SRC"/. "$FIRMWARE_DEST/"

## Step 6: Copy netplan config
if [ ! -f "$NETPLAN_SRC" ]; then
  echo "Netplan config missing: $NETPLAN_SRC"
  exit 1
fi
cp -u "$NETPLAN_SRC" "$NETPLAN_DEST"

## Step 7: Customize image (idempotentish)
if ! sudo virt-customize -a "$CVM_IMG" --run-command 'true'; then
  echo "virt-customize not working; check the environment/configuration."
  exit 1
fi

## Install `just`, `iperf3`, `redis-server`, `memcached` and `fio`, and inject the ssh key
## Setup the VirtFS mount configuration for sharing the files with the host
## Update the network configuration for the gdpr image in the CVM_eval project
sudo virt-customize -a "$CVM_IMG" -x \
  --root-password password:123456 \
  --edit '/etc/ssh/sshd_config:s/#PermitRootLogin prohibit-password/PermitRootLogin yes/' \
  --edit '/etc/ssh/sshd_config:s/PasswordAuthentication no/PasswordAuthentication yes/' \
  --run-command 'growpart /dev/sda 1 || true' \
  --run-command 'resize2fs /dev/sda1 || true' \
  --run-command 'ssh-keygen -A' \
  --run-command 'systemctl mask pollinate.service' \
  --run-command 'apt update && apt install -y just iperf3 redis-server memcached fio' \
  --run-command 'systemctl disable redis-server' \
  --run-command 'systemctl mask redis-server' \
  --ssh-inject root:file:"$SSH_KEY_PATH" \
  --run-command "echo 'GRUB_CMDLINE_LINUX_DEFAULT=\"\$GRUB_CMDLINE_LINUX_DEFAULT idle=poll\"' >> /etc/default/grub.d/50-cloudimg-settings.cfg" \
  --run-command "update-grub" \
  --copy-in "$NETPLAN_DEST:/etc/netplan/" \
  --run-command 'mkdir -p /share' \
  --append-line '/etc/fstab:share /share 9p trans=virtio,version=9p2000.L,rw,_netdev 0 0' \
  --append-line '/etc/modules-load.d/9p.conf:9p' \
  --append-line '/etc/modules-load.d/9p.conf:9pnet' \
  --append-line '/etc/modules-load.d/9p.conf:9pnet_virtio' \
  --smp "$(nproc)" \
  --memsize 16384

## Step 8: Go to the proper directory
cd CVM_eval

## Step 9: Set up the bridge and tap interfaces
nix develop -c just setup_bridge || true
nix develop -c just setup_tap || true

## Step 10: Run benchmarks only if the scripts exist and are executable
for script in experiment/bench_network.sh experiment/bench_storage.sh experiment/bench_swiotlb.sh; do
  if [ -x "$script" ] || [ -f "$script" ]; then
    sudo su -c "nix develop -c bash $script"
  else
    echo "WARNING: missing or not executable: $script"
  fi
done

nix develop -c bash experiment/plot_all.sh
