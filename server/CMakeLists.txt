set( SERVER ${PROJECT_NAME}_server)

set( SERVER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/server/include/)

set( PROTOCOL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/protocol)

set( SOURCES 
    src/main.cpp
)

add_executable(${SERVER}
    ${SOURCES}
)

target_include_directories(${SERVER} 
    PUBLIC 
    ${PROJECT_CONFIG_PATH}
    ${SERVER_INCLUDE_DIR}
    ${PROTOCOL_INCLUDE_DIR}
)

target_link_libraries(${SERVER}
    PUBLIC
    POOL
)
