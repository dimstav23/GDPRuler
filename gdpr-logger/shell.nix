{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    cmake
    gnumake
    openssl
    gtest
    zlib
  ];
}
