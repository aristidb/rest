CXX      := g++
CXXFLAGS := -g -W -Wall -Wno-long-long -pedantic -std=c++98 -DBOOST_SP_DISABLE_THREADS -I. -Itestsoon/include
LDFLAGS  := -static -L. -lrest -lboost_filesystem -lbz2 -lz
BUILDDIR := build

OS	:= $(shell uname)

LIBREST_SOURCES := context.cpp keywords.cpp response.cpp uri.cpp \
	host.cpp server.cpp logger.cpp datetime.cpp socket_device.cpp \
	http_headers.cpp config.cpp stream.cpp bzip2.cpp zlib.cpp \
	mapped_file.cpp
ifeq ($(OS), Darwin)
LIBREST_SOURCES := $(LIBREST_SOURCES) epoll.cpp
CXXFLAGS := $(CXXFLAGS) -DAPPLE
endif
LIBREST_OBJECTS := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(LIBREST_SOURCES))
LIBREST_DEPS    := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(LIBREST_SOURCES))

UNIT_SOURCES := unit.cpp filter_tests.cpp logger_tests.cpp http_headers_tests.cpp
UNIT_OBJECTS := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(UNIT_SOURCES))
UNIT_DEPS    := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(UNIT_SOURCES))

SOURCES  := $(wildcard *.cpp)
OBJECTS  := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(SOURCES))
DEPS     := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(SOURCES))

.PHONY: all rest
all: librest.a pipedump unit http-handler-test test1

rest: librest.a

librest.a: $(LIBREST_OBJECTS) $(LIBREST_DEPS)
	ar rcs librest.a $(LIBREST_OBJECTS)

pipedump: pipedump.cpp $(BUILDDIR)/pipedump.dep
	$(CXX) $(CXXFLAGS) pipedump.cpp -o pipedump

http-handler-test: librest.a http-handler-test.cpp $(BUILDDIR)/http-handler-test.dep
	$(CXX) $(CXXFLAGS) http-handler-test.cpp -o http-handler-test $(LDFLAGS)

unit: librest.a $(UNIT_OBJECTS) $(UNIT_DEPS)
	$(CXX) $(UNIT_OBJECTS) -o unit $(LDFLAGS)

test1: librest.a test1.cpp $(BUILDDIR)/test1.dep
	$(CXX) $(CXXFLAGS) test1.cpp -o test1 $(LDFLAGS)

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif

$(OBJECTS): $(BUILDDIR)/%.o: %.cpp $(BUILDDIR)/%.dep $(BUILDDIR)/.tag
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(DEPS): $(BUILDDIR)/%.dep: %.cpp $(BUILDDIR)/.tag
	$(CXX) $(CXXFLAGS) -MM $< -MT $@ -MT $(<:.cpp=.o) -o $@

%.tag:
	mkdir -p $(dir $(@))
	touch $@

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)
