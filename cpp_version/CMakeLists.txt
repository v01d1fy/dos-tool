cmake_minimum_required(VERSION 3.15)
project(dos_tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dos_tool
    main.cpp
    attacks/http_flood.cpp
    attacks/slowloris.cpp
    attacks/tcp_syn.cpp
    attacks/udp_flood.cpp
)

target_include_directories(dos_tool PRIVATE ${CMAKE_SOURCE_DIR})

if(WIN32)
    target_link_libraries(dos_tool PRIVATE ws2_32)
    target_compile_definitions(dos_tool PRIVATE _WIN32_WINNT=0x0601)
endif()

if(MSVC)
    target_compile_options(dos_tool PRIVATE /W3)
else()
    target_compile_options(dos_tool PRIVATE -Wall -Wextra -O2)
endif()
