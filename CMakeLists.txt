# 能兼容的最小 cmake 版本
cmake_minimum_required(VERSION 3.10)

# 定义项目名称
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME})

# c++ 标准
set(CMAKE_CXX_STANDARD 11)

# 设置头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 设置可执行文件输出目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 查找当前目录下的所有原文件
file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# 添加源文件到项目
add_executable(${PROJECT_NAME} ${SOURCES} ./main.cpp)