MODE ?= release
CXX ?= c++
.PHONY: all makefiles checkmakefiles clean cleanall test smoke
all: checkmakefiles
	$(MAKE) -C src MODE=$(MODE)

makefiles:
	cd src && opp_makemake -f --deep -o OpticalDataCenter

checkmakefiles:
	@test -f src/Makefile || { echo 'Run make makefiles in an OMNeT++ environment first.'; exit 1; }

clean:
	@if test -f src/Makefile; then $(MAKE) -C src MODE=$(MODE) clean; fi
	rm -rf build

cleanall: clean
	@if test -f src/Makefile; then $(MAKE) -C src MODE=debug clean; $(MAKE) -C src MODE=release clean; fi
	rm -f src/Makefile

test:
	mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc tests/scheduler_test.cc src/VermilionScheduler.cc -o build/scheduler_test
	./build/scheduler_test

smoke: all
	python3 tests/smoke.py
