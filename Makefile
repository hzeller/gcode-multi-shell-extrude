CXXFLAGS=-Wall -O2
LIBS=-lm

multi-shell-extrude: multi-shell-extrude.o rotational-polygon.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o : %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(LIBS)

clean:
	rm -f multi-shell-extrude
