CXXFLAGS=-Wall -O2
LIBS=-lm

multi-shell-extrude: multi-shell-extrude.cc
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f multi-shell-extrude
