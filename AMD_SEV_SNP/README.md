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
Import the [amd_sev_snp.nix](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/modules/amd_sev_snp.nix) (if you need only SNP) or 
[amd_sev_svsm.nix](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/modules/amd_sev_svsm.nix) (if you need additional svsm support) module in the server configuration. 
An example configuration is shown [here](https://github.com/TUM-DSE/doctor-cluster-config/blob/master/hosts/ryan.nix). 

This module sets the appropriate kernel version and parameters, and adds the mandatory kernel modules for SME, SEV-SNP, and, optionally, svsm .

**Note:** this setup has been tested with 
- kernel `5.19-rc6` sev-snp version provided by AMD ([link](https://github.com/mmisono/linux/tree/sev-snp-iommu-avic_5.19-rc6_v4-dev))
- kernel `6.1.0-rc4` svsm version provided by AMD ([link](https://github.com/AMDESE/linux/tree/svsm-preview-hv-v2))

### 2. Verify that SME, SEV and SEV-ES are enabled:
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
- You need to have cloud-config file and a network-config file for your VM, similar to those in the [cloud_configs](./cloud_configs/) folder.
- If you wish to have ssh connection to your VMs, you can adapt the cloud-config files and include your ssh keys, so that cloud-init sets them up automatically in the VM. Example cloud-init configurations that include the placeholders for ssh keys can be found [here](./cloud_configs/).
- The [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) script takes as a parameter the virtual bridge where the VMs will be connected to and modifies the IP prefix in the network configuration (given as a secord parameter) appropriately.

Follow the next set of commands to launch an SEV-SNP guest (tested with ubuntu 22.04 cloud img).
```
$ wget https://cloud-images.ubuntu.com/kinetic/current/kinetic-server-cloudimg-amd64.img 

$ mkdir images

$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./linux-svsm/scripts/usr/local/bin/qemu-img convert kinetic-server-cloudimg-amd64.img ./images/sev-server.img

$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH  ./linux-svsm/scripts/usr/local/bin/qemu-img resize ./images/sev-server.img +20G 

$ ./prepare_net_cfg.sh -br virbr0 -cfg ./cloud_configs/network-config-server.yml

$ sudo cloud-localds -N ./cloud_configs/network-config-server.yml ./images/server-cloud-config.iso ./cloud_configs/cloud-config-server

$ cp ./linux-svsm/scripts/usr/local/share/qemu/OVMF_CODE.fd ./OVMF_CODE_server.fd

$ cp ./linux-svsm/scripts/usr/local/share/qemu/OVMF_VARS.fd ./OVMF_VARS_server.fd
```

**Important note:** 
- Each VM requires a separate `.img` and `OVMF_*.fd` files.
- To avoid any problems, you have to use a distro with text-based installer, otherwise your launched VM might stuck ([issue](https://github.com/AMDESE/AMDSEV/issues/38)).

### 5. Launch an AMD SEV-SNP guest.
```
$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./launch-qemu.sh \
-hda ./images/sev-server.img \
-cdrom ./images/server-cloud-config.iso \
-sev-snp \
-bridge virbr0 \
-bios ./OVMF_CODE_server.fd \
-bios-vars ./OVMF_VARS_server.fd
```

**Important notes:**
- Be a bit patient, the network configuration above takes some seconds. If, in the meantime, you encounter a log-in prompt that does not accept your credentials, you can try Ctrl+C which will detach from the current tty and will allow the cloud-init to finish properly. Then you can log in normally.
It is a known "issue". 
- Follow the same process for the creation of a client vm (if you want/need to).
You need a different `.img`, and to adapt the network configuration appropriately to reserve a different IP.
Configuration examples are given in the [cloud_configs](./cloud_configs/) folder.

### 6. Inside the guest VM, verify that AMD SEV-SNP is enabled:
`sudo dmesg | grep snp -i ` should indicate `Memory Encryption Features active: AMD SEV SEV-ES SEV-SNP`

### 7. Networking: 
In step 5 above, we use the parameter `-bridge virbr0`, so that our VMs use the virtual network bridge `virbr0`. 
Our script [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) checks the given virtual bridge and adjust the prefix of the IP declared in the network configuration file. Example configuration files are given in the [cloud_configs](./cloud_configs/) folder. They are used mainly to pre-determine the IPs of the VMs in the network.

### Manual ssh connection setup
- After you make sure that networking works fine and you can reach the VM guest from the host, you can log-in the VM using ssh (after placing your ssh keys in the `~/.ssh/autorhized_keys` file of the guest VM).

### Useful links
- Sample cloud-config and network-config for cloud-init can be found [here](https://gist.github.com/itzg/2577205f2036f787a2bd876ae458e18e).
- Additional options of the cloud-config, such as running a specific command during initialization, can be found [here](https://www.digitalocean.com/community/tutorials/how-to-use-cloud-config-for-your-initial-server-setup)