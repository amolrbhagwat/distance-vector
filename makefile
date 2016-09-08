SHELL:=/bin/bash -O extglob

all: distvect.cpp
	g++ -std=c++11 -pthread -o distvect distvect.cpp

clean:
	rm !(makefile|distvect.cpp)
