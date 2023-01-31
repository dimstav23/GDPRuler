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
    linux.dev

    #for the sev-tool
    automake

    #general
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
    export KDIR=${linux.dev}/lib/modules/${linux.dev.modDirVersion}/build
    export PATH=${pythonEnv}/bin:$PATH
  ''; 
}

