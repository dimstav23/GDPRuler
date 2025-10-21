#!/bin/sh -i

mkdir -p build
cd build
cmake ..
make
cd ..

# considering small messages
printf "\nRunning domain becnhmark for 64 bytes messages"
./build/source/domain/domain  -c 1000000 -s 64
printf "\nRunning pipe becnhmark for 64 bytes messages"
./build/source/pipe/pipe      -c 1000000 -s 64
printf "\nRunning tcp becnhmark for 64 bytes messages"
./build/source/tcp/tcp        -c 1000000 -s 64

# considering medium messages
printf "\nRunning domain becnhmark for 1024 bytes messages"
./build/source/domain/domain  -c 1000000 -s 1024
printf "\nRunning pipe becnhmark for 1024 bytes messages"
./build/source/pipe/pipe      -c 1000000 -s 1024
printf "\nRunning tcp becnhmark for 1024 bytes messages"
./build/source/tcp/tcp        -c 1000000 -s 1024

# considering large messages
printf "\nRunning domain becnhmark for 4096 bytes messages"
./build/source/domain/domain  -c 1000000 -s 4096
printf "\nRunning pipe becnhmark for 4096 bytes messages"
./build/source/pipe/pipe      -c 1000000 -s 4096
printf "\nRunning tcp becnhmark for 4096 bytes messages"
./build/source/tcp/tcp        -c 1000000 -s 4096
