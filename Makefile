CXXFLAGS := -pthread -O2 -std=c++11
LDFLAGS := -pthread

spire: spire.o spirelayout.o getopt.o network.o
	$(CXX) $(LDFLAGS) $^ -o $@

spiredb: spiredb.o spirelayout.o getopt.o network.o
	$(CXX) $(LDFLAGS) $^ -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

getopt.o: getopt.h stringutils.h
network.o: network.h
spire.o: getopt.h network.h spirelayout.h stringutils.h
spiredb.o: getopt.h network.h spirelayout.h stringutils.h
spirelayout.o: spirelayout.h

clean:
	rm -f *.o
	rm -f spire
