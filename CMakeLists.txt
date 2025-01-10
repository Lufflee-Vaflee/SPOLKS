cmake_minimum_required(VERSION 3.11)
project(SPOVM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_CONFIG_PATH "")

function(generate_config)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(OS_ENV ${CMAKE_SYSTEM_NAME} )
    
        message(
            STATUS "Supported operating system spotted: ${CMAKE_SYSTEM_NAME}"
        )

        configure_file(
            ${PROJECT_SOURCE_DIR}/config/config.hpp.in 
            ${PROJECT_SOURCE_DIR}/config/gen/config.hpp
        )

        set(PROJECT_CONFIG_PATH ${PROJECT_SOURCE_DIR}/config/gen PARENT_SCOPE)
    else()
        message(
            FATAL_ERROR "Unsupported operating system spotted: ${CMAKE_SYSTEM_NAME}"
        )
    endif()
endfunction()

generate_config()

add_subdirectory(server)

add_subdirectory(client)

