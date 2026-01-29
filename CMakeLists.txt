cmake_minimum_required(VERSION 3.10)
project(moduoX)

# 设置最终生成的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息和C++11标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++14")

#设置全局链接库
set(LIBS pthread)

add_subdirectory(src)
# add_subdirectory(log)
# add_subdirectory(memory)