CXXFLAGS=-Wall

multi-shell-extrude: multi-shell-extrude.cc
	$(CXX) $(CXXFLAGS) -o $@ $< -lm

clean:
	rm -f multi-shell-extrude
