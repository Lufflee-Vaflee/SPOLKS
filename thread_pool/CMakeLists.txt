cmake_minimum_required(VERSION 3.11)
project(POOL)

set( SOURCES 
    src/ThreadPool.cpp
)

set( PUBLIC_HEADERS
    /include
)

add_library( ${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories( ${PROJECT_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

