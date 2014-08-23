CXXFLAGS=-Wall -O2 -frounding-math
LIBS=-lm -lCGAL_Core -lCGAL -lboost_thread -lmpfr -lgmp
OBJECTS=multi-shell-extrude.o rotational-polygon.o polygon-offset.o

multi-shell-extrude: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o : %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f multi-shell-extrude $(OBJECTS)
