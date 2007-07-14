CXX      := g++
CXXFLAGS := -g -W -Wall -Wno-long-long -pedantic -std=c++98 -DBOOST_SP_DISABLE_THREADS -I. -Itestsoon/include
LINK     := -static -L. -lrest
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

SOURCES  := $(wildcard *.cpp)
OBJECTS  := $(patsubst %.cpp, $(BUILDDIR)/%.o, $(SOURCES))
DEPS     := $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(SOURCES))

.PHONY: all rest
all: librest.a test1 pipedump unit http-handler-test

rest: librest.a

librest.a: $(LIBREST_OBJECTS) $(LIBREST_DEPS)
	ar rcs librest.a $(LIBREST_OBJECTS)

pipedump: pipedump.cpp pipedump.dep
	$(CXX) $(CXXFLAGS) pipedump.cpp -o pipedump

http-handler-test: librest.a http-handler-test.cpp http-handler-test.dep
	$(CXX) $(CXXFLAGS) 

unit:

test1: librest.a test1.cpp test1.dep
	$(CXX) $(CXXFLAGS) $(LINK) test1.cpp -o test 1

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
