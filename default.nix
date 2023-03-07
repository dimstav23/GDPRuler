with import <nixpkgs> { };
let
  pythonEnv = python3.withPackages (ps: [
      ps.pandas
      ps.pexpect
      ps.matplotlib
    ]);
  libraries = [ pixman zlib ];
in
mkShell {
  buildInputs = libraries;
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

    #for sev guest
    ninja
    glib
    nasm
    acpica-tools
    flex
    bison
    elfutils
    smatch
    rpm

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
    rocksdb
    clang-tools
    lz4
    bzip2
    snappy
    gflags
    boost

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

  # make install strips valueable libraries from our rpath
  LD_LIBRARY_PATH = lib.makeLibraryPath libraries;
  shellHook = ''
    export KDIR=${linuxPackages_latest.kernel.dev}/lib/modules/${linuxPackages_latest.kernel.dev.modDirVersion}/build
    export PATH=${pythonEnv}/bin:$PATH
  ''; 
}

