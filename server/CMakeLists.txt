set( SERVER ${PROJECT_NAME}_server)


set( SOURCES 
    src/main.cpp
)

set( HEADERS
)

add_executable(${SERVER}
    ${SOURCES}
    ${HEADERS}
)

#message("AAAAAAAAAAAAAAAAAA ${PROJECT_CONFIG_PATH}")

target_include_directories(${SERVER} 
    PUBLIC 
    ${PROJECT_CONFIG_PATH}
)

