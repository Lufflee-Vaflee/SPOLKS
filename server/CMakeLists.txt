set( SERVER ${PROJECT_NAME}_server)

set( SERVER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/server/include/)

set( PROTOCOL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/protocol)

set( THREAD_POOL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thread_pool)

set( SOURCES 
    src/main.cpp
)

add_executable(${SERVER}
    ${SOURCES}
)

message("AAAAAAAAAAAAAAAAAA ${CMAKE_SOURCE_DIR}")


target_include_directories(${SERVER} 
    PUBLIC 
    ${PROJECT_CONFIG_PATH}
    ${SERVER_INCLUDE_DIR}
    ${PROTOCOL_INCLUDE_DIR}
    ${THREAD_POOL_INCLUDE_DIR}
)

