cmake_minimum_required(VERSION 3.10)
project(proxy2tor C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS_RELEASE "-Oz -Wall -fno-ident -fdata-sections -ffunction-sections -fmerge-constants -s")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

include_directories(${INCLUDE_DIR})

file(GLOB SOURCES
    ${SRC_DIR}/*.c
)

add_executable(proxy2tor ${SOURCES})

if(WIN32)
    target_link_libraries(proxy2tor ws2_32)
else()
    target_link_libraries(proxy2tor pthread)
endif()
