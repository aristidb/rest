include_directories(/usr/local/include/boost-1_33_1)
include_directories(boost_asio_0_3_7)

add_library(rest context.cpp keywords.cpp response.cpp)

add_executable(test1 test1.cpp)
target_link_libraries(test1 rest)

add_executable(http-handler http-handler.cpp)
target_link_libraries(http-handler rest)
