CXX = g++
CXXFLAGS = -std=c++11 -O2

ourscheme: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: ourscheme
	./ourscheme < examples/demo.scm

clean:
	rm -f ourscheme

.PHONY: test clean
