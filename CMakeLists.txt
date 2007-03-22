# vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
include_directories(/usr/local/include/boost-1_33_1)

include_directories(boost_asio_0_3_7)
include_directories(testsoon/include)

add_definitions(-Wall -W)
#add_definitions(-fvisibility=hidden) #TODO

add_library(rest SHARED
  rest_test.cpp
  http-handler.cpp
  context.cpp
  keywords.cpp
  response.cpp
  uri.cpp
  boundary_reader.cpp
  host.cpp
  server.cpp
  logger.cpp
)
target_link_libraries(rest boost_iostreams boost_thread)

add_executable(test1 test1.cpp)
target_link_libraries(test1 rest)

add_executable(http-handler-test http-handler-test.cpp)
target_link_libraries(http-handler-test rest)

add_executable(pipedump pipedump.cpp)

add_executable(unit unit.cpp) #add what you test
target_link_libraries(unit rest)
