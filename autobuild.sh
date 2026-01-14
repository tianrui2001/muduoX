#!/bin/bash

# 使用 set -e 可以在任何命令失败时立即退出脚本
set -e

# 脚本所在的根目录
PROJECT_ROOT=$(pwd)

# 如果没有build目录，则创建一个
if [ ! -d "$PROJECT_ROOT/build" ]; then
    mkdir "$PROJECT_ROOT/build"
fi

# 进入build目录
cd "$PROJECT_ROOT/build"

rm -rf ./*

echo "--- Building project ---"
cmake ..
# 使用 nproc 获取CPU核心数来并行编译
make -j$(nproc)

cd ..

if [ ! -d /usr/include/muduoX ]; then
    mkdir /usr/include/muduoX
fi

for header in $(ls $PROJECT_ROOT/include); do
    cp $PROJECT_ROOT/include/$header /usr/include/muduoX/
done

cp $PROJECT_ROOT/lib/libmuduoX.so /usr/lib

ldconfig
echo "muduoX installed successfully!"