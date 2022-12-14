with import <nixpkgs> { };
mkShell {
  nativeBuildInputs = [
    #for the kernel module build
    cpuid
    dmidecode
    msr
    linux.dev

    #for the sev-tool
    automake

    bashInteractive
    dnsmasq
    pkg-config
    qemu
    libvirt
    virt-manager
    vim
    libuuid
    nasm
    file
    bridge-utils
    cloud-utils
    python3

    #redis specific packages
    tcl
    tcltls
    openssl
    jemalloc

    #rockdb specific packages
    clang-tools
    lz4
    bzip2
    snappy
    zlib
    gflags

    #GDPRBench specific packages
    maven
    jdk11
    python2
    ];
  
  shellHook = ''
    export KDIR=${linux.dev}/lib/modules/${linux.dev.modDirVersion}/build
  ''; 
}

