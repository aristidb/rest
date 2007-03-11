# vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
include_directories(/usr/local/include/boost-1_33_1)

include_directories(boost_asio_0_3_7)
include_directories(testsoon/include)

add_definitions(-Wall -W)

add_library(rest SHARED
  context.cpp keywords.cpp response.cpp uri.cpp boundary_reader.cpp)
target_link_libraries(rest boost_iostreams)

add_executable(test1 test1.cpp)
target_link_libraries(test1 rest)

add_executable(http-handler http-handler.cpp)
target_link_libraries(http-handler rest boost_thread)

add_executable(pipedump pipedump.cpp)

add_executable(unit unit.cpp) #add what you test
