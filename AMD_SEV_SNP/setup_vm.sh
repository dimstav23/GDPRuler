#!/bin/bash

# VM setup script

# Install dependencies
sudo apt-get update
sudo apt-get install -y automake bash pkg-config vim uuid-dev nasm file tcl tcl-tls openssl libjemalloc-dev libhiredis-dev  liblz4-dev libbz2-dev libsnappy-dev zlib1g-dev libgflags-dev cmake git cppcheck doxygen codespell libabsl-dev libssl-dev clang net-tools dnsmasq libboost-all-dev librocksdb-dev clang-tools clang-tidy maven

# Choose the installed clang (currently gcc-12 has issues with libboost)
CLANG_VERSION=$(clang --version | grep version | cut -d' ' -f4 | cut -d'.' -f1)
# Set CC related environment variables
export CC="/usr/bin/clang-${CLANG_VERSION}"
export CPP="/usr/bin/clang-cpp-${CLANG_VERSION}"
export CXX="/usr/bin/clang++-${CLANG_VERSION}"

# Navigate to the home folder
cd /home/ubuntu

# Fetch and install libredis++:
if [ -d "redis-plus-plus" ]; then
  echo "redis-plus-plus already exists -- skip cloning"
else
  git clone https://github.com/sewenew/redis-plus-plus.git
  git checkout f779c71ff20829b0de7aa91e5dae2f9411c9af0d
  cd redis-plus-plus
  mkdir build
  cd build
  cmake -DREDIS_PLUS_PLUS_CXX_STANDARD=17 ..
  make
  sudo make install
fi

# Fetch GDPRuler
cd /home/ubuntu
if [ -d "GDPRuler" ]; then
  echo "GDPRuler already exists -- skip cloning"
else
  git clone https://github.com/dimstav23/GDPRuler.git
  cd GDPRuler
  git submodule update --init

  # Compile redis to get the redis-server exec:
  cd /home/ubuntu/GDPRuler/KVs/redis/
  make BUILD_TLS=yes MALLOC=libc
  # Optional command to test the success of the installation
  # make test

  # Compile the controller (release version)
  cd /home/ubuntu/GDPRuler/controller
  cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
  cmake --build build

  # Optional -- workload generation
  sudo apt-get install -y maven python2
  cd /home/ubuntu/GDPRuler/ycsb_trace_generator
  ./workload_generator.sh
fi