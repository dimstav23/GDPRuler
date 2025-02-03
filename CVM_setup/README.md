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
Import the [amd_sev_snp.nix](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/modules/amd_sev_snp.nix).
An example configuration is shown [here](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/hosts/rose.nix). 

This module sets the appropriate kernel version and parameters, and adds the mandatory kernel modules for SME, SEV-SNP, and, optionally, svsm .

**Note:** this setup has been tested with (currently only `SEV-ES`) 
- kernel `6.6.0-rc1` sev-snp version provided by AMD ([link](https://github.com/AMDESE/linux/tree/snp-host-latest)) -- the latest tested commit is [here](https://github.com/AMDESE/linux/commit/5a170ce1a08259ac57a9074e1e7a170d6b8c0cda)

### 2. Verify that SME, SEV and SEV-ES are enabled:
- `dmesg | grep sev` should include `sev enabled` in its output.
- `dmesg | grep -i SEV-ES` should indicate that `SEV-ES` is supported and the number of SEV ASIDs.
- `dmesg | grep -i SEV-SNP` should indicate that `SEV-SNP` is enabled and the number of ASIDs.

### 3. Prepare the host toolchain (OVMF)
Compile the custom OVMF provided by AMD:
```
$ cd AMDSEV
$ bash build.sh ovmf
```

**Note:** 

For SNP, this setup has been tested with 
- `ovmf`: snp-latest branch provided by AMD ([link to our fork](https://github.com/dimstav23/amd-ovmf/tree/ovmf_fix_snapshot_31_01_2025)) -- the latest tested commit is [here](https://github.com/dimstav23/amd-ovmf/commit/27bd7da5ca29dd2cdc3186489e95354716fad71e)


### 4. Prepare an AMD SEV-SNP guest.
- You need to have a network-config file (`.yaml`) for your VM, similar to those in the [network_configs](./network_configs/) folder.
- The [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) script takes as a parameter the virtual bridge where the VMs will be connected to and modifies the IP prefix in the network configuration (given as a secord parameter) appropriately.

Follow the next set of commands from the `AMD_SEV_SNP` directory to launch an SEV-SNP guest (tested with ubuntu 22.04 cloud img).
```
$ wget https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img

$ mkdir images

$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH qemu-img convert noble-server-cloudimg-amd64.img ./images/controller.img

$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH qemu-img resize ./images/controller.img +20G

$ bash prepare_net_cfg.sh -br virbr0 -cfg ./network_configs/netplan-controller.yml

$ mkdir -p firmware/controller

$ cp ./AMDSEV/usr/local/share/qemu/OVMF.fd ./firmware/controller/OVMF.fd
```

For convenience, we wrap these operations in a single script ([GDPRuler_VMs_setup.sh](./GDPRuler_VMs_setup.sh))to setup a controller, a server and a client image.

**Important note:** 
- Each VM requires a separate `.img` and `OVMF.fd` files.
- To avoid any problems, you have to use a distro with text-based installer, otherwise your launched VM might stuck ([issue](https://github.com/AMDESE/AMDSEV/issues/38)).

### 5. Launch an AMD SEV-SNP guest.
```
$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH bash AMDSEV/launch-qemu.sh \
-hda images/controller.img \
-sev-snp \
-bridge virbr0 \
-bios firmware/controller
```

**IMPORTANT:** 
As of 25/1/2024 the synchronized versions of `linux kernel`, `qemu` and `ovmf` do not allow to run `SNP` VMs on a host kernel lower than
`version 6.7`. Therefore, if you have an older kernel (<=`6.6`), change the -sev-snp parameter above to -sev-es till you update the 
host kernel to a version >=`6.7`.


**Important notes:**
- Follow the same process for the creation of a client vm (if you want/need to).
You need a different `.img`, and to adapt the network configuration appropriately to reserve a different IP.
Network configuration examples are given in the [network_configs](./network_configs/) folder.

### 6. Inside the guest VM, verify that AMD SEV-SNP is enabled:
`sudo dmesg | grep snp -i` should indicate `Memory Encryption Features active: AMD SEV SEV-ES SEV-SNP`

### 7. Networking: 
In step 5 above, we use the parameter `-bridge virbr0`, so that our VMs use the virtual network bridge `virbr0`. 
Typically, this is set up if you are using `libvirt`.
If it does not exist, you can create and configure it by using the `bridge-utils` package.
An example is shown below:
```
sudo brctl addbr virbr0
sudo brctl stp virbr0 on
sudo ifconfig virbr0 up
sudo ifconfig virbr0 192.168.122.1 netmask 255.255.255.0
```
Our script [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) checks the given virtual bridge and adjusts the prefix of the IP declared in the network configuration file. Example configuration files are given in the [network_configs](./network_configs/) folder. They are used mainly to pre-determine the IPs of the VMs in the network.

### Manual ssh connection setup
- After you make sure that networking works fine and you can reach the VM guest from the host, you can log-in the VM using ssh (after placing your ssh keys in the `~/.ssh/autorhized_keys` file of the guest VM).

### Useful links
- AMD [host kernels](https://github.com/AMDESE/linux) -- check branch names for each feature (e.g., SEV, ES, SNP)
- [QEMU](https://github.com/AMDESE/qemu) provided by AMD / or the [private fork](https://github.com/dimstav23/amd-qemu) that is used
- [OVMF](https://github.com/AMDESE/ovmf) provided by AMD / or the [private fork](https://github.com/dimstav23/amd-ovmf) that is used