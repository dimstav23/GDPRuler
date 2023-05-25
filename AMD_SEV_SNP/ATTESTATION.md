# Steps to perform the attestion of an SEV-SNP guest:

To get an attestation report:

1. Launch an SEV-SNP VM as shown [here](https://github.com/dimstav23/GDPRuler/tree/main/AMD_SEV_SNP).

2. Inside the VM, build the host & guest SNP kernels. To do so,
```
# git clone https://github.com/AMDESE/AMDSEV.git
# cd AMDSEV
# git checkout snp-latest
# ./build.sh --package kernel
# sudo cp kvm.conf /etc/modprobe.d/
```
Tested with [this](https://github.com/AMDESE/AMDSEV/tree/b04cde73313687cbd6c21c444cfd6f7ee8d28062) commit.

3. Install the build kernel on the SNP VM:
```
# cd snp-release-{date}/linux/guest/
# sudo dpkg -i *.deb
```

4. Shutdown the VM and start it again to load the new kernel. **IMPORTANT:** SEV VMs [do not support reboots](https://github.com/AMDESE/AMDSEV/issues/157).

5. For the attestation reports the device `/dev/sev-guest` is used. You will notice it's not there.
Therefore, you now need to run `sudo modprobe sev-guest` to load the kernel module.
For more information about this undocumented phenomenon of SEV-SNP guests, you can have a look at [this thesis](https://kth.diva-portal.org/smash/get/diva2:1737821/FULLTEXT01.pdf).

6. Now, go back to the base directory and install the [`sev-guest`](https://github.com/AMDESE/sev-guest/tree/main) utility. 
Importantly, since the `sev-guest` repository is poorly maintained, you need to perform the steps listed [here](https://arkivm.github.io/sev/2022/11/25/running-amd-svsm/) to successfully compile it.
More precisely, since you need to unpack the guest kernel’s libc-dev inside sev-guest using the `dpkg -x linux-libc-dev_{version}_amd64.deb ./libc-guest` where this `.deb` package is in the `AMDSEV/snp-release-{date}/linux/guest/` directory you used earlier.
Then, perform the presented adaptations of the Makefile and run `make`.

7. To get a simple attestation report, go to sev-guest directory and run `sudo ./sev-guest-get-report report.bin`.
You can also parse the report to dump its contents `sudo ./sev-guest-parse-report report.bin`.

**Known current issues:**
1. Cert issue when building the latest guest kernel. ([SOLVED](https://github.com/AMDESE/AMDSEV/issues/156))
2. `sev-guest-get-report` fails to provide a second report. ([PENDING](https://github.com/AMDESE/sev-guest/issues/40))