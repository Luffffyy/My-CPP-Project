cmake_minimum_required(VERSION 3.12)
project(CppHttpServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES server.cpp)

add_executable(server ${SOURCES})

# Multi-threading library
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)

# Windows Winsock linking
if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(server PRIVATE /W4)
else()
    target_compile_options(server PRIVATE -Wall -Wextra -O2)
endif()
