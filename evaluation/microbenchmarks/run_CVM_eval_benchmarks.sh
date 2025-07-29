#!/bin/bash
set -x
set -e
set -u
set -o pipefail

# mkdir -p CVM_eval/gdpr_setup
# wget https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
# qemu-img convert noble-server-cloudimg-amd64.img my-ubuntu-vm.img

# # Check the existence of the required CVM image and firmware files
# # and copy them to the appropriate directories in the CVM_eval project.
# mkdir -p CVM_eval/gdpr_setup/images
# mkdir -p CVM_eval/gdpr_setup/ovmf

# cp my-ubuntu-vm.img CVM_eval/gdpr_setup/images/gdpr.img
# qemu-img resize CVM_eval/gdpr_setup/images/gdpr.img +10G
# cp ../../CVM_setup/firmware/gdpr/* CVM_eval/gdpr_setup/ovmf
# cp ../../CVM_setup/network_configs/netplan-gdpr-cvm-eval.yaml CVM_eval/gdpr_setup/

# # Install `just`, `iperf3`, `redis-server`, `memcached` and `fio`, and inject the ssh key
# sudo virt-customize -a "CVM_eval/gdpr_setup/images/gdpr.img" -x \
#   --root-password password:123456 \
#   --edit '/etc/ssh/sshd_config:s/#PermitRootLogin prohibit-password/PermitRootLogin yes/' \
#   --edit '/etc/ssh/sshd_config:s/PasswordAuthentication no/PasswordAuthentication yes/' \
#   --run-command 'growpart /dev/sda 1' \
#   --run-command 'resize2fs /dev/sda1' \
#   --run-command 'ssh-keygen -A' \
#   --run-command 'systemctl mask pollinate.service' \
#   --run-command 'apt update && apt install -y just iperf3 redis-server memcached fio' \
#   --ssh-inject root:file:"CVM_eval/nix/ssh_key.pub" \
#   --run-command "echo 'GRUB_CMDLINE_LINUX_DEFAULT=\"\$GRUB_CMDLINE_LINUX_DEFAULT idle=poll\"' >> /etc/default/grub.d/50-cloudimg-settings.cfg" \
#   --run-command "update-grub" \
#   --smp $(nproc) \
#   --memsize 16384

# # Setup the VirtFS mount configuration for sharing the files with the host
# sudo virt-customize -a "CVM_eval/gdpr_setup/images/gdpr.img" -x \
#   --run-command 'mkdir -p /share' \
#   --append-line '/etc/fstab:share /share 9p trans=virtio,version=9p2000.L,rw,_netdev 0 0' \
#   --append-line '/etc/modules-load.d/9p.conf:9p' \
#   --append-line '/etc/modules-load.d/9p.conf:9pnet' \
#   --append-line '/etc/modules-load.d/9p.conf:9pnet_virtio'

# # Update the network configuration for the gdpr image in the CVM_eval project
# sudo virt-customize -a "CVM_eval/gdpr_setup/images/gdpr.img" -x \
#   --copy-in CVM_eval/gdpr_setup/netplan-gdpr-cvm-eval.yaml:/etc/netplan/

# # Apply the CVM_eval patch to the CVM_eval project
cd CVM_eval
# # git apply ../CVM_eval.patch

# # Set up the bridge and tap interfaces
# nix develop -c just setup_bridge || true
# nix develop -c just setup_tap || true

# Run the CVM_eval benchmarks script
sudo su -c "nix develop -c bash experiment/bench_network.sh"
sudo su -c "nix develop -c bash experiment/bench_storage.sh"
sudo su -c "nix develop -c bash experiment/bench_swiotlb.sh"