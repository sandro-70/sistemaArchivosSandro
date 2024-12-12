cmake_minimum_required(VERSION 3.10)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
include_directories(include)
# Include the directory where json.hpp is located
add_executable(MyProject src/main.cpp src/BlockDevice.cpp)


