cmake_minimum_required(VERSION 3.21)
project(AppleMetal CXX)

set(CMAKE_CXX_STANDARD 17)

# Get SPIRV Support
include(FetchContent)
FetchContent_Declare(
        SPIRV-Cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
)
FetchContent_MakeAvailable(SPIRV-Cross)
add_library(spirv-support STATIC src/SPIRV_Loader.cpp)
target_link_libraries(spirv-support spirv-cross-msl spirv-cross-glsl spirv-cross-core spirv-cross-reflect)

# Get metal-cpp
FetchContent_Declare(
        metal-cpp
        URL https://developer.apple.com/metal/cpp/files/metal-cpp_macOS12_iOS15.zip
)

FetchContent_MakeAvailable(metal-cpp)
include_directories(${metal-cpp_SOURCE_DIR})

include_directories(include)
add_executable(main main.cpp)
target_link_libraries(main spirv-support "-framework QuartzCore" "-framework Metal" "-framework Foundation")

