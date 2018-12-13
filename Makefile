spire: spire.o getopt.o
	$(CXX) -pthread $^ -o $@

.cpp.o:
	$(CXX) -pthread -O2 -c $< -o $@

getopt.o: getopt.h stringutils.h
spire.o: getopt.h

clean:
	rm -f *.o
	rm -f spire
