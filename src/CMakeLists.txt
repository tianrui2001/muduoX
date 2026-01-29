# 收集 src 下的 cpp
file(GLOB SRC_DIR_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# 收集 log 下的 cpp
file(GLOB LOG_DIR_FILES ${CMAKE_SOURCE_DIR}/log/*.cpp)
# 收集 memory 下的 cpp
file(GLOB MEM_DIR_FILES ${CMAKE_SOURCE_DIR}/memory/*.cpp)

# 合并生成一个库
add_library(muduoX SHARED ${SRC_DIR_FILES} ${LOG_DIR_FILES} ${MEM_DIR_FILES})

#设置头文件路径
target_include_directories(muduoX PUBLIC ${CMAKE_SOURCE_DIR}/include)