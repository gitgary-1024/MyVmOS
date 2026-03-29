cmake_minimum_required(VERSION 3.10)
project(SimpleCompiler VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 收集所有源文件
set(COMPILER_SOURCES
    src/Token.cpp
    src/AST/ASTnode.cpp
    src/AST/AST.cpp
    src/AST/ASTPrinter.cpp
    src/IR/IRbase.cpp
    src/IR/IR.cpp
    src/IR/IRprinter.cpp
    src/IR/IRToAssembly.cpp
    src/File.cpp
)

# 创建静态库
add_library(compiler_lib STATIC ${COMPILER_SOURCES})

# 添加头文件搜索路径
target_include_directories(compiler_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/AST
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IR
)

# 定义调试宏（可选）
target_compile_definitions(compiler_lib PRIVATE _DEBUG)

# 创建测试可执行文件
add_executable(test_ir_to_assembly tests/test_ir_to_assembly.cpp)
target_link_libraries(test_ir_to_assembly compiler_lib)
set_target_properties(test_ir_to_assembly PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# 创建可执行文件（仅当独立编译时）
if(NOT TARGET myos_lib)
    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} compiler_lib)
    
    # 设置输出目录
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
    )
endif()
