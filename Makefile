CXXFLAGS := -pthread -O2 -std=c++11
LDFLAGS := -pthread

PQXX_CFLAGS = $(shell pkg-config --cflags libpqxx)
PQXX_LDFLAGS = $(shell pkg-config --libs libpqxx)

all: spire

spire: console.o getopt.o network.o spire.o spirelayout.o spirepool.o stringutils.o types.o
	$(CXX) $(LDFLAGS) $^ -o $@

spiredb: getopt.o network.o spiredb.o spirelayout.o stringutils.o
	$(CXX) $(LDFLAGS) $(PQXX_LDFLAGS) $^ -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -c $< -o $@

console.o: console.h
getopt.o: getopt.h stringutils.h
network.o: network.h
spire.o: console.h getopt.h network.h spire.h spirelayout.h spirepool.h stringutils.h types.h
spiredb.o: getopt.h network.h spiredb.h spirelayout.h stringutils.h types.h
spiredb.o: EXTRA_CXXFLAGS = $(PQXX_CFLAGS)
spirelayout.o: spirelayout.h types.h
stringutils.o: stringutils.h
types.o: types.h

clean:
	rm -f *.o
	rm -f spire spiredb
