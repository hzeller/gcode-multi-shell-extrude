CXXFLAGS=-Wall -O2
LIBS=-lm
OBJECTS=multi-shell-extrude.o rotational-polygon.o polygon-offset.o

multi-shell-extrude: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o : %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(LIBS)

clean:
	rm -f multi-shell-extrude $(OBJECTS)
