CXXFLAGS := -pthread -O2 -std=c++11
LDFLAGS := -pthread

spire: spire.o spirelayout.o getopt.o
	$(CXX) $(LDFLAGS) $^ -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

getopt.o: getopt.h stringutils.h
spire.o: getopt.h
spirelayout.o: spirelayout.h

clean:
	rm -f *.o
	rm -f spire
