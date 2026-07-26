# makefile: build and run the tests for the shared utilities of 90_common
#
# Like the CPU core tests of 10.05_cputest these are built by the HOST compiler
# and run on the build machine: under ./crossco the "demo" binary is built by
# the ARM cross compiler while these run natively on the x64 host, and under
# ./compile.sh on the BeagleBone the host compiler is the ARM one and the same
# rules work unchanged. Never use $(BBB_CC) or $(CC) here.
#
# 90_common is target independent and has no _u/_q split, so one set of tests
# serves both platforms.
#
# Targets:
#	all / test	build what is out of date, then run the tests
#	retest		drop the stamps and run everything again
#	clean		remove objects, binaries and stamps
#
# A test binary may also be run by hand for its verbose output, e.g.
#	../4_deploy/getopt2test -v

OBJDIR = $(abspath ../4_deploy)
STAMPDIR = $(OBJDIR)/stamps
SRCDIR = $(abspath ../src)

HOST_CXX ?= g++

# GCC_ROOT points at the ARM cross toolchain and is exported by crosscompile.env,
# which ./crossco sources before it gets here. gcc treats a non-empty GCC_ROOT as
# its own installation prefix, so leaving it set makes the HOST g++ look for its
# internal headers under the ARM toolchain and fail with
#	/usr/include/stdio.h: fatal error: stddef.h: No such file or directory
unexport GCC_ROOT

CXXFLAGS = \
	-std=c++11 \
	-O2 -Wall -Wextra -Wshadow \
	-I$(SRCDIR) \
	-c

# One test binary per unit under test: <name>test from <name>_test.cpp plus the
# unit's own source out of ../src.
TESTS = getopt2

PROGS = $(foreach t,$(TESTS),$(OBJDIR)/$(t)test)
STAMPS = $(foreach t,$(TESTS),$(STAMPDIR)/$(t).ok)

.PHONY: all test retest clean

# else make deletes the objects and even the test binaries again as
# intermediates of the pattern rule chain below, and relinks them every time.
.SECONDARY:

all: test

test: $(STAMPS)
	@echo "90_common tests up to date: $(TESTS)"

retest:
	rm -rf $(STAMPDIR)
	$(MAKE) test

clean:
	rm -rf $(OBJDIR)

$(OBJDIR) $(STAMPDIR):
	mkdir -p $@

# A stamp depends on its test binary only, and that on the unit under test. So
# an edit to getopt2.cpp re-runs the parser tests, and an unrelated build runs
# nothing.
$(STAMPDIR)/%.ok: $(OBJDIR)/%test | $(STAMPDIR)
	$<
	@touch $@

$(OBJDIR)/%test: $(OBJDIR)/%_test.o $(OBJDIR)/%.o
	$(HOST_CXX) -o $@ $^

$(OBJDIR)/%_test.o: %_test.cpp $(SRCDIR)/%.hpp | $(OBJDIR)
	$(HOST_CXX) $(CXXFLAGS) $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.hpp | $(OBJDIR)
	$(HOST_CXX) $(CXXFLAGS) $< -o $@
