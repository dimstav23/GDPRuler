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

### 3. Prepare the host toolchain
Compile the custom OVMF and QEMU provided by AMD:
```
$ cd AMDSEV
$ git apply ../AMDSEV.patch
$ bash build.sh qemu
$ bash build.sh ovmf
```

**Note:** 

For SNP, this setup has been tested with 
- `qemu`: snp-latest branch provided by AMD ([link to our fork](https://github.com/dimstav23/amd-qemu/tree/snp-latest)) -- the latest tested commit is [here](https://github.com/dimstav23/amd-qemu/commit/b6ee1218e6c9b98a556841615dd10d094e648393)
- `ovmf`: snp-latest branch provided by AMD ([link to our fork](https://github.com/dimstav23/amd-ovmf/tree/snp-latest)) -- the latest tested commit is [here](https://github.com/dimstav23/amd-ovmf/commit/09fbe92dc545779671d7fd89a5bd4f1b14f7e69b)


### 4. Prepare an AMD SEV-SNP guest.
- You need to have cloud-config file and a network-config file for your VM, similar to those in the [cloud_configs](./cloud_configs/) folder.
- If you wish to have ssh connection to your VMs, you can adapt the cloud-config files and include your ssh keys, so that cloud-init sets them up automatically in the VM. Example cloud-init configurations that include the placeholders for ssh keys can be found [here](./cloud_configs/).
- The [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) script takes as a parameter the virtual bridge where the VMs will be connected to and modifies the IP prefix in the network configuration (given as a secord parameter) appropriately.

Follow the next set of commands from the `AMD_SEV_SNP` directory to launch an SEV-SNP guest (tested with ubuntu 22.04 cloud img).
```
$ wget https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-amd64.img

$ mkdir images

$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./AMDSEV/usr/local/bin/qemu-img convert jammy-server-cloudimg-amd64.img ./images/sev-server.img

$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH  ./AMDSEV/usr/local/bin/qemu-img resize ./images/sev-server.img +30G

$ bash prepare_net_cfg.sh -br virbr0 -cfg ./cloud_configs/network-config-server.yml

$ sudo cloud-localds -N ./cloud_configs/network-config-server.yml ./images/server-cloud-config.iso ./cloud_configs/cloud-config-server

$ mkdir -p OVMF_files/server

$ cp ./AMDSEV/usr/local/share/qemu/OVMF_CODE.fd ./OVMF_files/server/OVMF_CODE.fd

$ cp ./AMDSEV/usr/local/share/qemu/OVMF_VARS.fd ./OVMF_files/server/OVMF_VARS.fd
```

For convenience, we wrap these operations in a single script ([GDPRuler_VMs_setup.sh](./GDPRuler_VMs_setup.sh))to setup both a server and a client image.
Note that the actual setup will be performed on the first launch of the VM -- so, please be patient :)

**Important note:** 
- Each VM requires a separate `.img` and `OVMF_*.fd` files.
- To avoid any problems, you have to use a distro with text-based installer, otherwise your launched VM might stuck ([issue](https://github.com/AMDESE/AMDSEV/issues/38)).

### 5. Launch an AMD SEV-SNP guest.
```
$ sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH bash AMDSEV/launch-qemu.sh \
-hda images/sev-server.img \
-cdrom images/server-cloud-config.iso \
-sev-snp \
-bridge virbr0 \
-bios OVMF_files/server
```

**IMPORTANT:** 
As of 25/1/2024 the synchronized versions of `linux kernel`, `qemu` and `ovmf` do not allow to run `SNP` VMs on a host kernel lower than
`version 6.7`. Therefore, if you have an older kernel (<=`6.6`), change the -sev-snp parameter above to -sev-es till you update the 
host kernel to a version >=`6.7`.


**Important notes:**
- Be a bit patient, the network configuration above takes some seconds. If, in the meantime, you encounter a log-in prompt that does not accept your credentials, you can try Ctrl+C which will detach from the current `tty` and will allow the cloud-init to finish properly. Then you can log in normally.
It is a known "issue". 
- Follow the same process for the creation of a client vm (if you want/need to).
You need a different `.img`, and to adapt the network configuration appropriately to reserve a different IP.
Configuration examples are given in the [cloud_configs](./cloud_configs/) folder.

### 6. Inside the guest VM, verify that AMD SEV-SNP is enabled:
`sudo dmesg | grep snp -i` should indicate `Memory Encryption Features active: AMD SEV SEV-ES SEV-SNP`

### 7. Networking: 
In step 5 above, we use the parameter `-bridge virbr0`, so that our VMs use the virtual network bridge `virbr0`. 
Our script [`prepare_net_cfg.sh`](./prepare_net_cfg.sh) checks the given virtual bridge and adjust the prefix of the IP declared in the network configuration file. Example configuration files are given in the [cloud_configs](./cloud_configs/) folder. They are used mainly to pre-determine the IPs of the VMs in the network.

### 8. Attestation (maybe outdated due to updated kernel):
For more information about the attestation process, please consult our [dedicated documentation](./ATTESTATION.md).
Sample attestation process is also presented in our [sev-snp-attestation](./sev-snp-attestation/) submodule.

### Manual ssh connection setup
- After you make sure that networking works fine and you can reach the VM guest from the host, you can log-in the VM using ssh (after placing your ssh keys in the `~/.ssh/autorhized_keys` file of the guest VM).

### Useful links
- Sample cloud-config and network-config for cloud-init can be found [here](https://gist.github.com/itzg/2577205f2036f787a2bd876ae458e18e).
- Additional options of the cloud-config, such as running a specific command during initialization, can be found [here](https://www.digitalocean.com/community/tutorials/how-to-use-cloud-config-for-your-initial-server-setup)
- AMD [host kernels](https://github.com/AMDESE/linux) -- check branch names for each feature (e.g., SEV, ES, SNP)
- [QEMU](https://github.com/AMDESE/qemu) provided by AMD / or the [private fork](https://github.com/dimstav23/amd-qemu) that is used
- [OVMF](https://github.com/AMDESE/ovmf) provided by AMD / or the [private fork](https://github.com/dimstav23/amd-ovmf) that is used