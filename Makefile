CXXFLAGS=-Wall -O2
LIBS=-lm
OBJECTS=multi-shell-extrude.o rotational-polygon.o polygon-offset.o \
	printer.o config-values.o third_party/clipper.o

multi-shell-extrude: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o : %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f multi-shell-extrude $(OBJECTS)
