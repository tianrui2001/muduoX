#获取当前目录下的所有源文件
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

#创建静态链接库
add_library(muduoX SHARED ${SRC_FILES})

#设置头文件路径
target_include_directories(muduoX PUBLIC ${CMAKE_SOURCE_DIR}/include)