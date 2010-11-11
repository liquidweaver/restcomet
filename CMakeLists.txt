cmake_minimum_required(VERSION 2.8)
project(restcomet)
find_package(Boost COMPONENTS regex thread REQUIRED )
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

add_library(restcomet restcomet.cpp)
add_executable( rest_test main.cpp)
add_executable( stresstest_restcomet stresstest_restcomet.cpp )
set(CMAKE_CXX_FLAGS "-g -Wall")
target_link_libraries( rest_test ${Boost_LIBRARIES} restcomet )
target_link_libraries( stresstest_restcomet ${Boost_LIBRARIES} restcomet )

