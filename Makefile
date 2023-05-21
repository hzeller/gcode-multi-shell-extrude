CXXFLAGS=-Wextra -Wall -std=c++11 -Wno-unused-parameter -Wno-deprecated-copy -Wno-class-memaccess -O2
LIBS=-lm
OBJECTS=multi-shell-extrude.o rotational-polygon.o polygon-offset.o \
	printer.o config-values.o vector2d.o third_party/clipper.o

multi-shell-extrude: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o : %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f multi-shell-extrude $(OBJECTS)
