with import <nixpkgs> { };
let
  pythonEnv = python3.withPackages (ps: [
      ps.pandas
      ps.pexpect
      ps.matplotlib
    ]);
in
mkShell {
  nativeBuildInputs = [
    #for the kernel module build
    cpuid
    dmidecode
    msr
    msr-tools
    linuxPackages_latest.kernel.dev
    unzip

    #for the sev-tool
    autoconf
    automake

    #general
    bc
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

    #redis specific packages
    tcl
    tcltls
    openssl
    jemalloc
    hiredis
    redis-plus-plus

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

    #for the controller
    cmake
    git
    clang
    cppcheck
    doxygen
    codespell
    abseil-cpp

    ];
  
  shellHook = ''
    export KDIR=${linuxPackages_latest.kernel.dev}/lib/modules/${linuxPackages_latest.kernel.dev.modDirVersion}/build
    export PATH=${pythonEnv}/bin:$PATH
  ''; 
}

