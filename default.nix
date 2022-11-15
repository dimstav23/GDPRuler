with import <nixpkgs> { };
mkShell {
  nativeBuildInputs = [
    bashInteractive
    # to run tests
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
    ];
}

