# Steps to enable and launch an ubuntu-based SEV-SNP guest (applied in our NixOs servers - TUM Cluster) :

### BIOS preparation
- You have to enable the SME and SNP options in your BIOS settings.
To do so in [`ryan`](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/docs/hosts/ryan.md) and 
[`graham`](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/docs/hosts/graham.md),
access their mgmt interface via ssh and run the following:
```
set BIOS.ProcSettings.Sme Enabled
set BIOS.ProcSettings.Snp Enabled
jobqueue create BIOS.Setup.1-1
```
and then reboot the server.
- For SEV ES you have to enable IOMMU support and set the minimum SEV ASIDs value in the BIOS.
To do so in [`ryan`](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/docs/hosts/ryan.md) and 
[`graham`](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/docs/hosts/graham.md), 
run the following after accessing their mgmt interface:
```
set BIOS.ProcSettings.IommuSupport Enabled
set BIOS.ProcSettings.CpuMinSevAsid 128
jobqueue create BIOS.Setup.1-1
```
and then reboot the server.
For more information regarding the parameter for CPU mininmum SEV ASIDs specifically in our machines,
look [here](https://www.dell.com/support/manuals/en-us/idrac9-lifecycle-controller-v4.x-series/idrac_4.00.00.00_racadm_ar_referenceguide/bios.procsettings.cpuminsevasid-(read-or-write)?guid=guid-4bdaeaa7-d054-4fd1-bd84-0cd71d7aec1e&lang=en-us).

### 1. Use the dedicated host kernel and enable SEV-SNP in the desired server:
Import the [amd_sev_snp.nix](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/modules/amd_sev_snp.nix) module in the server configuration. 
An example configuration is shown [here](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/hosts/ryan.nix). 

This module sets the appropriate kernel version and parameters, and adds the mandatory kernel modules for SME and SEV-SNP.

**Note:** this setup has been tested with kernel `5.19-rc6` sev-snp version provided by AMD, with binutils patches.
You can find the kernel [here](https://github.com/mmisono/linux/tree/sev-snp-iommu-avic_5.19-rc6_v4-dev).

### 2. Verify that SME, SEV and SEV-ES are enabled:
- `dmesg | grep SME` should indicate `AMD Memory Encryption Features active: SME` and
- `dmesg | grep sev` should include `sev enabled` in its output.
- `dmesg | grep -i SEV-ES` should indicate that `SEV-ES` is supported and the number of SEV ASIDs.
- `dmesg | grep -i SEV-SNP` should indicate that `SEV-SNP` is enabled and the number of ASIDs.

### 3. Prepare the host toolchain
Compile the custom OVMF and QEMU provided by AMD:
```
$ cd linux-svsm/scripts
$ ./build.sh qemu
$ ./build.sh ovmf
```

### 4. Prepare an AMD SEV-SNP guest.
You need to have cloud-config file and a network-config file for your VM, similar to those in the [cloud_configs](./cloud_configs/) folder.
Follow the next set of commands to launch an SEV-SNP guest (tested with ubuntu 22.04 cloud img).
```
$ wget https://cloud-images.ubuntu.com/kinetic/current/kinetic-server-cloudimg-amd64.img 

$ mkdir images

$ sudo ./linux-svsm/scripts/usr/local/bin/qemu-img convert kinetic-server-cloudimg-amd64.img ./images/sev-server.img

$ sudo ./linux-svsm/scripts/usr/local/bin/qemu-img resize ./images/sev-server.img +20G 

$ ./prepare_net_cfg.sh virbr0 ./cloud_configs/network-config-server.yml

$ sudo cloud-localds -N ./cloud_configs/network-config-server.yml ./images/server-cloud-config.iso ./cloud_configs/cloud-config-server

$ cp /linux-svsm/scripts/usr/local/share/qemu/OVMF_CODE.fd ./OVMF_CODE_server.fd

$ cp /linux-svsm/scripts/usr/local/share/qemu/OVMF_VARS.fd ./OVMF_VARS_client.fd
```

**Important note:** 
- The [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) script takes as a parameter the virtual bridge where the VMs will be connected to and modifies the IP prefix in the network configuration (given as a secord parameter) appropriately.
- Each VM requires a separate `.img` and `OVMF_*.fd` files.

### 5. Launch an AMD SEV-SNP guest.
```
$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./launch-qemu.sh 
-hda ./images/sev-server.img \
-cdrom ./images/server-cloud-config.iso \
-sev-snp \
-bridge virbr0 \
-bios ./OVMF_CODE_server.fd \
-bios-vars ./OVMF_VARS_server.fd
```

**Important note:** 
- Follow the same process for the creation of a client vm (if you want/need to).
You need a different `.img`, and to adapt the network configuration appropriately to reserve a different IP.
Configuration examples are given in the [cloud_configs](./cloud_configs/) folder.

### 6. Inside the guest VM, verify that AMD SEV-SNP is enabled:
`sudo dmesg | grep snp -i ` should indicate `Memory Encryption Features active: AMD SEV SEV-ES SEV-SNP`

### 7. Networking: 
In step 5 above, we use the parameter `-bridge virbr0`, so that our VMs use the virtual network bridge `virbr0`. 
Our script [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) checks the given virtual bridge and adjust the prefix of the IP declared in the network configuration file. Example configuration files are given in the [cloud_configs](./cloud_configs/) folder. They are used mainly to pre-determine the IPs of the VMs in the network.

### Notes
- After you make sure that networking works fine and you can reach the VM guest from the host, you can log-in the VM using ssh (after placing your ssh keys in the `~/.ssh/autorhized_keys` file of the guest VM) 

### TODO
- Add authorization keys inside the cloud-config itself
