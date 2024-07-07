cmake_minimum_required(VERSION 3.10)
project(SPOLKS)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -Wpedantic)

set(SOURCES
    src/main.cpp
)

add_executable(${PROJECT_NAME} 
    ${SOURCES}
)

