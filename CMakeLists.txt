# vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
include_directories(/usr/local/include/boost-1_33_1)

#include_directories(boost_asio_0_3_7)
include_directories(testsoon/include)

add_definitions(-Wall -W)

set(CMAKE_EXE_LINKER_FLAGS "-static" CACHE STRING "Linker flags" FORCE)

add_library(rest
  rest_test.cpp
  context.cpp
  keywords.cpp
  response.cpp
  uri.cpp
  host.cpp
  server.cpp
  logger.cpp
  datetime.cpp
  socket_device.cpp

  # Boost.Iostreams
  bzip2.cpp
  zlib.cpp
  mapped_file.cpp
)
target_link_libraries(rest z bz2)

add_executable(test1 test1.cpp)
target_link_libraries(test1 rest)

add_executable(http-handler-test http-handler-test.cpp)
target_link_libraries(http-handler-test rest)

add_executable(pipedump pipedump.cpp)

add_executable(unit
  unit.cpp
  filter_tests.cpp
  logger_tests.cpp
)
target_link_libraries(unit rest)
