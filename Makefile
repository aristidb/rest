CXX         := g++
CXXSTDFLAGS := -pipe -W -Wall -Wno-long-long -pedantic -std=c++98 -DBOOST_SP_DISABLE_THREADS -I. -Itestsoon/include
CXXDBGFLAGS := -g3 -ggdb3 -DDEBUG
CXXOPTFLAGS := -O3 -DNDEBUG -Wno-unused
LDFLAGS     := -static -L. -lrest -lboost_filesystem -lbz2 -lz
AR          := ar
ARFLAGS     := rcs
BUILDDIR    := build
override BUILDDIR := $(strip $(BUILDDIR))

OS	    := $(shell uname)
ifeq ($(OS), Darwin)
LDFLAGS     := -L. ./librest.a -dynamic -lz -lbz2 -lboost_filesystem
CXXOPTFLAGS := $(CXXOPTFLAGS) -fast -mcpu=G4 -mtune=G4
CXXSTDFLAGS := $(CXXSTDFLAGS) -DAPPLE
endif

#CXXFLAGS    := $(CXXSTDFLAGS) $(CXXDBGFLAGS)
CXXFLAGS    := $(CXXSTDFLAGS) $(CXXOPTFLAGS)

LIBREST_SOURCES := context.cpp keywords.cpp response.cpp uri.cpp \
	host.cpp server.cpp logger.cpp datetime.cpp socket_device.cpp \
	http_headers.cpp config.cpp stream.cpp request.cpp \
	bzip2.cpp zlib.cpp mapped_file.cpp
ifeq ($(OS), Darwin)
LIBREST_SOURCES := $(LIBREST_SOURCES) epoll.cpp
endif
LIBREST_OBJECTS := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(LIBREST_SOURCES))
LIBREST_DEPS    := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(LIBREST_SOURCES))

UNIT_SOURCES := unit.cpp filter_tests.cpp http_headers_tests.cpp datetime-test.cpp config_tests.cpp
UNIT_OBJECTS := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(UNIT_SOURCES))
UNIT_DEPS    := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(UNIT_SOURCES))

SOURCES  := $(wildcard *.cpp)
OBJECTS  := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(SOURCES))
DEPS     := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(SOURCES))

LIBREST  :=librest.a

BINARIES := $(LIBREST) unit http-handler-test test1 boundary-filter-bench

.PHONY: all rest
all: $(BINARIES)

rest: $(LIBREST)

$(LIBREST): $(LIBREST_OBJECTS) $(LIBREST_DEPS)
	$(AR) $(ARFLAGS) librest.a $(LIBREST_OBJECTS)

http-handler-test: $(LIBREST) http-handler-test.cpp $(BUILDDIR)/http-handler-test.dep
	$(CXX) $(CXXFLAGS) http-handler-test.cpp -o http-handler-test $(LDFLAGS)

unit: librest.a $(UNIT_OBJECTS) $(UNIT_DEPS)
	$(CXX) $(UNIT_OBJECTS) -o unit $(LDFLAGS)

test1: librest.a test1.cpp $(BUILDDIR)/test1.dep
	$(CXX) $(CXXFLAGS) test1.cpp -o test1 $(LDFLAGS)

boundary-filter-bench: boundary-filter-bench.cpp $(BUILDDIR)/boundary-filter-bench.dep
	$(CXX) $(CXXSTDFLAGS) $(CXXOPTFLAGS) $(LDFLAGS) boundary-filter-bench.cpp -o boundary-filter-bench

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif

$(OBJECTS): $(BUILDDIR)/%.o: %.cpp $(BUILDDIR)/%.dep $(BUILDDIR)/.tag
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(DEPS): $(BUILDDIR)/%.dep: %.cpp $(BUILDDIR)/.tag
	@echo Dep: $<
	@$(CXX) $(CXXFLAGS) -MM $< -MT $@ -MT $(<:.cpp=.o) -o $@ || \
		echo $@ $(<:.cpp=.o): $< > $@

%.tag:
	@echo Create build-directory
	@mkdir -p $(dir $(@))
	@touch $@

.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(BINARIES)
