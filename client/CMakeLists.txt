project(${PROJECT_NAME}_client)

set( CLIENT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/client/include/)

set( PROTOCOL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/protocol)

set( SOURCES 
    src/main.cpp
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME} 
    PUBLIC 
    ${PROJECT_CONFIG_PATH}
    ${PROTOCOL_INCLUDE_DIR}
)
