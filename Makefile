CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude

.PHONY: all clean

all: server client

server: src/server.cpp include/common.hpp
	$(CXX) $(CXXFLAGS) -o $@ src/server.cpp

client: src/client.cpp include/common.hpp
	$(CXX) $(CXXFLAGS) -o $@ src/client.cpp

clean:
	rm -f server client *.o
