# Steps to enable and launch an ubuntu-based SEV guest (applied in our NixOs servers - TUM Cluster) :

### 1. Enable SEV in the desired server:
Import the [amd_sev.nix](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/modules/amd_sev.nix) module in the server configuration. 
An example configuration is shown [here](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/hosts/graham.nix). 
This module sets the appropriate kernel parameters and adds the mandatory kernel modules for SME and SEV.

To verify that your host can run secure guests, run the following:
```
sudo virt-host-validate | grep secure
```

### 2. Verify that SME and SEV are enabled:
- `dmesg | grep SME` should indicate `AMD Memory Encryption Features active: SME` and
- `dmesg | grep sev` should include `sev enabled` in its output.
- `sudo virsh domcapabilities | grep sev` should indicate that `sev` is enabled for libvirt.

### 3. Follow the instructions presented [here](https://github.com/Masheenist/AMDSEV/blob/main/README.md) or [here](https://docs.ovh.com/us/en/dedicated/enable-and-use-amd-sme-sev/) to launch an SEV guest.
The set of commands is also listed here for simplicity:
```
$ wget https://cloud-images.ubuntu.com/focal/current/focal-server-cloudimg-amd64.img

$ mkdir images

$ sudo qemu-img convert focal-server-cloudimg-amd64.img ./images/sev-guest.img

$ cat >cloud-config <<EOF
#cloud-config
password: amd_sev
chpasswd: { expire: False }
ssh_pwauth: False
EOF

$ sudo cloud-localds ./images/sev-guest-cloud-config.iso cloud-config

$ cat > ~/.config/libvirt/qemu.conf <<EOF
#UEFI OVMF CODE & VARS for nix
nvram = ["/run/libvirt/nix-ovmf/OVMF_CODE.fd:/run/libvirt/nix-ovmf/OVMF_VARS.fd"]
EOF

$ sudo virt-install \
--name sev-guest \
--memory 4096 \
--memtune hard_limit=4563402 \
--boot uefi \
--disk ./images/sev-guest.img,device=disk,bus=scsi \
--disk ./images/sev-guest-cloud-config.iso,device=cdrom \
--os-variant ubuntu20.04 \
--import \
--controller type=scsi,model=virtio-scsi,driver.iommu=on \
--controller type=virtio-serial,driver.iommu=on \
--network network=default,model=virtio,driver.iommu=on \
--memballoon driver.iommu=on \
--graphics none \
--launchSecurity sev \
--network bridge=virbr0
```
Have in mind that you might want to add the [policy](https://documentation.suse.com/sles/15-SP1/html/SLES-amd-sev/index.html) parameter of AMD SEV depending on your purpose.

**Notes:**
- Tested with `virt-manager 4.0.0`. Currently, `virt-manager 4.1.0` introduces a weird bug. 
- If you receive an error mentioning that the `default network` is not active, you can check it through `sudo virsh net-list --all` and then 
use `sudo virsh net-start default` to start it.
- If you receive kvm persmission errors, try adding yourself to the `kvm` group (or your respectively named group) for getting the permissions
by using `sudo usermod -a -G {kvm_group_name} {your_user_name}`.

**Optional:** you can use `sudo qemu-img resize` to increase the available disk space inside the VM.
For example: `sudo qemu-img resize ./images/sev-guest.img +10G`

### 4. Inside the guest VM, verify that AMD SEV is enabled:
`dmesg | grep SEV` should indicate `AMD Secure Encrypted Virtualization (SEV) active`

### 5. For networking to work properly: 
You have to inform networkd service **not** to manage the `vnet*` interfaces created by `libvirt`. To achieve that, add a similar rule to your server configuration, as shown in this [example](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/hosts/graham.nix)
Additionally, make sure that your VM gets an IP address by running `sudo virsh net-dhcp-leases`.
If no IP is assigned to your VM, follow the instructions [here](https://wiki.libvirt.org/page/Networking) and use `sudo virsh net-update` to assign an IP to your VM according to its MAC and name. For instance:
```
virsh net-update default add ip-dhcp-host \
          "<host mac='52:54:00:00:00:01' \
           name='bob' ip='192.168.122.45' />" \
           --live --config
```

### 6. To perform further functionality-proof testing:
1. To test SME and TSME, look [here](https://github.com/dimstav23/amd-mem-encryption-tests/tree/nix_path_test).
2. To verify the AMD SEV encryption, look [here](./amd-sev-encryption-tests/).

### Notes
- To create a domain and get a console to the VM: `sudo virsh create --file amdsev_demo.xml --console`
- To enable a console for the guest VM, check [here](https://serverfault.com/questions/364895/virsh-vm-console-does-not-show-any-output).
- To get a console on a running VM: `sudo virsh console <sev-guest-domain-name>`
If it seems to get stuck, just press Enter.
- After you make sure that networking works fine and you can reach the VM guest from the host, you can log-in the VM using ssh (after placing your ssh keys in the `~/.ssh/autorhized_keys` file of the guest VM) 
- The aforementioned process can also be performed using the [`plain_vm.xml`](./plain_vm.xml) provided here. Be aware that you have to perform again
the process described in **step 3** where you should modify the `sev-guest` prefixed files/images.
- To delete a domain, run `sudo virsh undefine --nvram "name of VM"`.