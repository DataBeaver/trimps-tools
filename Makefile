CXXFLAGS := -pthread -O2 -std=c++11
LDFLAGS := -pthread

PQXX_CFLAGS = $(shell pkg-config --cflags libpqxx)
PQXX_LDFLAGS = $(shell pkg-config --libs libpqxx)

all: spire perks

spire: console.o getopt.o network.o spire.o spirecore.o spirelayout.o spirepool.o stringutils.o types.o
	$(CXX) $(LDFLAGS) $^ -o $@

spiredb: getopt.o network.o spiredb.o spirelayout.o stringutils.o
	$(CXX) $(LDFLAGS) $(PQXX_LDFLAGS) $^ -o $@

perks: getopt.o perks.o stringutils.o types.o
	$(CXX) $(LDFLAGS) $^ -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -c $< -o $@

console.o: console.h
getopt.o: getopt.h stringutils.h
network.o: network.h
perks.o: getopt.h stringutils.h types.h
spire.o: console.h getopt.h network.h spire.h spirecore.h spirelayout.h spirepool.h stringutils.h types.h
spirecore.o: spirecore.h stringutils.h types.h
spiredb.o: getopt.h network.h spirecore.h spiredb.h spirelayout.h stringutils.h types.h
spiredb.o: EXTRA_CXXFLAGS = $(PQXX_CFLAGS)
spirelayout.o: spirecore.h spirelayout.h types.h
stringutils.o: stringutils.h
types.o: types.h

clean:
	rm -f *.o
	rm -f spire spiredb
