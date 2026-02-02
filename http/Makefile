# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -g 

# 头文件路径
# -I. : 包含当前目录
# -I../include : 包含 muduoX 库的头文件目录 (假设在上一级的 include 目录)
INCLUDES = -I. -I../include

# 库文件路径
# -L../lib : 假设 libmuduoX.so 生成在上一级的 lib 目录
LDFLAGS = -L../lib

# 需要链接的库
# -lmuduoX : 你的核心网络库
# -lpthread : 线程库
# -luring : io_uring 库 (muduoX 依赖它)
LIBS = -lmuduoX -lpthread -luring

# 目标可执行文件名
TARGET = httpserver

# 自动获取当前目录下所有的 .cpp 文件
SRCS = $(wildcard *.cpp)

# 将 .cpp 文件列表转换为 .o 文件列表
OBJS = $(patsubst %.cpp, %.o, $(SRCS))

# 默认目标
all: $(TARGET)

# 链接步骤
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)
	@echo "Build successful: $(TARGET)"

# 编译步骤 (.cpp -> .o)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理目标
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean