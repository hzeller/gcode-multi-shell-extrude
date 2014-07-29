CXXFLAGS=-Wall

multi-shell-extrude: multi-shell-extrude.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f multi-shell-extrude
