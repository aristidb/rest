CXX         := g++
CXXSTDFLAGS := -pipe -W -Wall -Wno-long-long -pedantic -std=c++98 -DBOOST_SP_DISABLE_THREADS -Iinclude -Itestsoon/include
CXXDBGFLAGS := -g3 -ggdb3 -DDEBUG
CXXOPTFLAGS := -O3 -DNDEBUG -Wno-unused
LDFLAGS     := -static -L. -lrest -lboost_filesystem$(BOOST_SUFFIX) -lboost_iostreams$(BOOST_SUFFIX) -lbz2 -lz
AR          := ar
ARFLAGS     := rcs
BUILDDIR    := build
override BUILDDIR := $(strip $(BUILDDIR))

OS	    := $(shell uname)
ifeq ($(OS), Darwin)
LDFLAGS     := -L. ./librest.a -dynamic -lz -lbz2 -lboost_filesystem$(BOOST_SUFFIX) -lboost_iostreams$(BOOST_SUFFIX)
CXXOPTFLAGS := $(CXXOPTFLAGS) -fast -mcpu=G4 -mtune=G4
CXXSTDFLAGS := $(CXXSTDFLAGS) -DAPPLE
endif

CXXFLAGS    := $(CXXSTDFLAGS) $(CXXDBGFLAGS)
#CXXFLAGS    := $(CXXSTDFLAGS) $(CXXOPTFLAGS)

LIBREST_SOURCES := $(wildcard src/*.cpp)

ifeq ($(OS), Darwin)
LIBREST_SOURCES := $(LIBREST_SOURCES) src/compat/epoll.cpp
endif

LIBREST_OBJECTS := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(LIBREST_SOURCES))
LIBREST_DEPS    := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(LIBREST_SOURCES))

UNIT_SOURCES := test/unit.cpp test/filter_tests.cpp test/test1.cpp \
	test/http_headers_tests.cpp test/datetime-test.cpp \
	test/config_tests.cpp test/http_connection.cpp
UNIT_OBJECTS := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(UNIT_SOURCES))
UNIT_DEPS    := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(UNIT_SOURCES))

SOURCES  := $(wildcard *.cpp) $(wildcard src/*.cpp) $(wildcard test/*.cpp) $(wildcard src/compat/*.cpp)
OBJECTS  := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(SOURCES))
DEPS     := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(SOURCES))

LIBREST  := librest.a

BINARIES := $(LIBREST) unit http-handler-test boundary-filter-bench

.PHONY: all rest
all: $(BINARIES)

rest: $(LIBREST)

$(LIBREST): $(LIBREST_OBJECTS) $(LIBREST_DEPS)
	$(AR) $(ARFLAGS) librest.a $(LIBREST_OBJECTS)

http-handler-test: $(BUILDDIR)/test/http-handler-test.o $(LIBREST) $(BUILDDIR)/test/http-handler-test.dep
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

unit: $(UNIT_OBJECTS) $(LIBREST) $(UNIT_DEPS)
	$(CXX) $(UNIT_OBJECTS) -o unit $(LDFLAGS)

boundary-filter-bench: $(BUILDDIR)/test/boundary-filter-bench.o $(BUILDDIR)/test/boundary-filter-bench.dep
	$(CXX) $(CXXSTDFLAGS) $(CXXOPTFLAGS) $(LDFLAGS) $< -o $@

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif

$(OBJECTS): $(BUILDDIR)/%.o: %.cpp $(BUILDDIR)/%.dep
	@echo Build $@
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(DEPS): $(BUILDDIR)/%.dep: %.cpp
	@echo Dep: $<
	@mkdir -p $(dir $(@))
	@$(CXX) $(CXXFLAGS) -MM $< -MT $@ -MT $(<:.cpp=.o) -o $@ || \
		echo $@ $(<:.cpp=.o): $< > $@

.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(BINARIES)
