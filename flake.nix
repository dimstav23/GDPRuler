{
  description = "GDPRuler";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    fenix = {
      url = "github:nix-community/fenix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, flake-utils, fenix }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        overlays = [
          (final: prev: {
            python2 = prev.python2.override {
              packageOverrides = python-self: python-super: {
                setuptools = python-super.setuptools.overrideAttrs (old: {
                  meta = old.meta // { insecure = false; };
                });
              };
            };
          })
        ];
        pkgs = import nixpkgs { 
          inherit system overlays;
          config = { 
            allowInsecure = true;
            permittedInsecurePackages = [
              "python-2.7.18.8"
            ];
          };
        };
        pythonEnv = pkgs.python3.withPackages (ps: with ps; [
          pandas
          pexpect
          matplotlib
          seaborn
        ]);
        libraries = with pkgs; [ pixman zlib zstd glib libpng snappy elfutils libslirp ];
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = libraries;
          nativeBuildInputs = with pkgs; [
            expect
            numactl
            libguestfs
            guestfs-tools
            gdb
            zmqpp
            zeromq
            #for the kernel module build
            # cpuid
            # dmidecode
            # msr
            # msr-tools
            # linuxPackages_latest.kernel.dev
            # unzip
            # rpm
            #for the sev-tool
            # autoconf
            # automake
            #for sev guest
            ninja
            nasm
            acpica-tools
            flex
            bison
            elfutils
            #smatch
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
            # Note: to use stable, just replace `default` with `stable`
            # fenix.packages.${system}.default.toolchain
          ];

          LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath libraries;
          shellHook = ''
            export PATH=${pythonEnv}/bin:$PATH
          '';

          RUST_BACKTRACE = 1;
        };
      }
    );
}
