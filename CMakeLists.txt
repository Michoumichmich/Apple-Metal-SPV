cmake_minimum_required(VERSION 3.21)
project(AppleMetal CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(include/metal-cpp)

add_executable(main main.cpp)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework QuartzCore -framework Metal -framework Foundation")

