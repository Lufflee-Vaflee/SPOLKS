cmake_minimum_required(VERSION 3.14)
project(POOL VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES 
    src/ThreadPool.cpp
)

set(PUBLIC_HEADERS
    /include
)

add_library(${PROJECT_NAME}
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

# Testing setup
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare (
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

# Add tests
add_executable(${PROJECT_NAME}_test 
    test/thread_pool_test.cpp
)

target_link_libraries(${PROJECT_NAME}_test PRIVATE 
    gtest_main 
    gmock_main 
    ${PROJECT_NAME}
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_test)

