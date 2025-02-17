project(${PROJECT_NAME}_server)

set( SERVER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/server/include/)

set( PROTOCOL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/protocol)

set( SOURCES 
    src/main.cpp
    src/ClientHandler.cpp
    src/AcceptHandler.cpp
    src/ClientService.cpp
    src/SocketAccess.cpp
    src/SessionData.cpp
    src/ServerLinux.cpp
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME} 
    PUBLIC 
    ${PROJECT_CONFIG_PATH}
    ${SERVER_INCLUDE_DIR}
    ${PROTOCOL_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    POOL
)
