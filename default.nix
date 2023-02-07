with import <nixpkgs> { };
let
  pythonEnv = python3.withPackages (ps: [
      ps.pandas
      ps.pexpect
      ps.matplotlib
    ]);
  virt_4_0_0_pkgs = import (builtins.fetchTarball {
        url = "https://github.com/NixOS/nixpkgs/archive/ee01de29d2f58d56b1be4ae24c24bd91c5380cea.tar.gz";
    }) {};
in
mkShell {
  nativeBuildInputs = [
    #for the kernel module build
    cpuid
    dmidecode
    msr
    msr-tools
    linuxPackages_latest.kernel.dev

    #for the sev-tool
    automake

    #general
    bashInteractive
    dnsmasq
    pkg-config
    qemu
    libvirt
    virt_4_0_0_pkgs.virt-manager
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

