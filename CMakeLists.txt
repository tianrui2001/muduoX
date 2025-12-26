cmake_minimum_required(VERSION 3.10)
project(moduoX)

# 设置最终生成的路径
set(LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息和C++11标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

aux_source_directory(. SRC_LIST)
# 生成动态库：muoduoX
add_library(muduoX SHARED ${SRC_LIST})