CC = g++
CFLAGS = -Wall -O2

all: direct_server direct_client direct_setup proxy_server proxy_client proxy_setup

direct_server: direct_server.cpp
	$(CC) $(CFLAGS) -o direct_server direct_server.cpp

direct_client: direct_client.cpp
	$(CC) $(CFLAGS) -o direct_client direct_client.cpp

direct_setup: direct_setup.cpp
	$(CC) $(CFLAGS) -o direct_setup direct_setup.cpp

proxy_server: proxy_server.cpp
	$(CC) $(CFLAGS) -o proxy_server proxy_server.cpp

proxy_client: proxy_client.cpp
	$(CC) $(CFLAGS) -o proxy_client proxy_client.cpp

proxy_setup: proxy_setup.cpp
	$(CC) $(CFLAGS) -o proxy_setup proxy_setup.cpp

run: all
	./direct_setup
	./proxy_setup

clean:
	rm -f direct_server direct_client direct_setup proxy_server proxy_client proxy_setup

