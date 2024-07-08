cmake_minimum_required(VERSION 3.10)
project(SPOLKS)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -Wpedantic)

set(CLIENT_SOURCES
    client/main.cpp
)

set(SERVER_SOURCES
    server/main.cpp
)

add_executable("${PROJECT_NAME}_CLIENT"
    ${CLIENT_SOURCES}
)

add_executable("${PROJECT_NAME}_SERVER"
    ${SERVER_SOURCES}
)

