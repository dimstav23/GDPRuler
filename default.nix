with import <nixpkgs> { };
let
  pythonEnv = python3.withPackages (ps: [
      ps.pandas
      ps.pexpect
      ps.matplotlib
      ps.seaborn
    ]);
  fenix = callPackage
    (fetchFromGitHub {
      owner = "nix-community";
      repo = "fenix";
      # commit from: 2023-03-03
      rev = "e2ea04982b892263c4d939f1cc3bf60a9c4deaa1";
      hash = "sha256-AsOim1A8KKtMWIxG+lXh5Q4P2bhOZjoUhFWJ1EuZNNk=";
    })
    { };
  libraries = [ pixman zlib zstd glib libpng snappy elfutils libslirp ];
in
mkShell {
  buildInputs = libraries;
  nativeBuildInputs = [
    expect
    numactl
    libguestfs
    guestfs-tools

    gdb
    #for the kernel module build
    cpuid
    dmidecode
    msr
    msr-tools
    linuxPackages_latest.kernel.dev
    unzip
    rpm

    #for the sev-tool
    autoconf
    automake

    #for sev guest
    ninja
    nasm
    acpica-tools
    flex
    bison
    elfutils
    smatch
    rpm
    libslirp

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
    openssl

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
  
    #for the snpguest -- rust nightly is required
    # cargo
    # rustup
    # Note: to use stable, just replace `default` with `stable`
    fenix.default.toolchain
  ];

  # make install strips valueable libraries from our rpath
  LD_LIBRARY_PATH = lib.makeLibraryPath libraries;
  shellHook = ''
    export KDIR=${linuxPackages_latest.kernel.dev}/lib/modules/${linuxPackages_latest.kernel.dev.modDirVersion}/build
    export PATH=${pythonEnv}/bin:$PATH
  '';

  # Set Environment Variables
  RUST_BACKTRACE = 1;
}

