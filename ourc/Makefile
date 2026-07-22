# OurC P1 build
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall

ourc.exe: ourc.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

.PHONY: ourc-test clean
ourc-test: ourc.exe
	@echo "== p1_demo.in =="; ./ourc.exe < tests/p1_demo.in
	@echo "== p1_err.in ==";  ./ourc.exe < tests/p1_err.in

clean:
	rm -f ourc.exe build.err
