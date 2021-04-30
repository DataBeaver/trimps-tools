CXXFLAGS := -pthread -O2 -std=c++11 -DWITH_128BIT=1
LDFLAGS := -pthread

PQXX_CFLAGS = $(shell pkg-config --cflags libpqxx)
PQXX_LDFLAGS = $(shell pkg-config --libs libpqxx)

all: spire

spire: console.o getopt.o spire.o spirecore.o spirelayout.o spirepool.o stringutils.o types.o
	$(CXX) $(LDFLAGS) $^ -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -c $< -o $@

console.o: console.h
getopt.o: getopt.h stringutils.h
spire.o: console.h getopt.h spire.h spirecore.h spirelayout.h spirepool.h stringutils.h types.h
spirecore.o: spirecore.h stringutils.h types.h
spirelayout.o: spirecore.h spirelayout.h types.h
stringutils.o: stringutils.h
types.o: types.h

clean:
	rm -f *.o
	rm -f spire
