# Verify AMD SEV encryption

1. Spawn 2 vms, one with `AMD SEV` enabled and one non-secure VM, 
as it is explained [here](../). 
For this tutorial, we assume that the AMD SEV VM is named `sev-guest`
and the non-secure VM is named `plain-guest`.

2. Verify the secure launch of the AMD SEV VM. You can compare the 
output of the following commands:
```
sudo virsh domlaunchsecinfo --domain sev-guest
sudo virsh domlaunchsecinfo --domain plain-guest
```

3. Copy the [`test.c](./test.c) file inside the VMs and compile it.
The test code allocates a string in stack and sets it to "AMDSEV\0".
In the end, it performs an endless loop so that the value remains allocated
in the VM memory.

4. From the host machine, create the memory dumps of the 2 VMs as follows:
```
sudo virsh dump --memory-only sev-guest --file ./sev_guest_mem --verbose
sudo virsh dump --memory-only plain-guest --file ./plain_guest_mem --verbose
```

5. Check if the string "AMDSEV" sequence exists in the memory dumps:
```
sudo grep "AMDSEV" -r ./sev_guest_mem
sudo grep "AMDSEV" -r ./plain_guest_mem
```
The `grep` command should find matches only in the memory of the non-secure VM
since the AMD SEV VM memory is encrypted.