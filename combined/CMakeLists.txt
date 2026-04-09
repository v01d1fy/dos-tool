cmake_minimum_required(VERSION 3.16)
project(dos_tool C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Source files ──────────────────────────────────────────────
set(C_SOURCES
    c_modules/http_flood.c
    c_modules/udp_flood.c
)

set(CXX_SOURCES
    cpp_modules/slowloris.cpp
    main.cpp
)

# ── Rust build ────────────────────────────────────────────────
set(RUST_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rust_modules")

# Detect if using MinGW (GNU) vs MSVC to pick correct Rust target
if(WIN32)
    if(MINGW)
        set(RUST_TARGET "x86_64-pc-windows-gnu")
        set(RUST_LIB_NAME "libtcp_syn.a")
        set(RUST_LIB_PATH "${RUST_MODULE_DIR}/target/${RUST_TARGET}/release/${RUST_LIB_NAME}")
    else()
        set(RUST_TARGET "x86_64-pc-windows-msvc")
        set(RUST_LIB_NAME "tcp_syn.lib")
        set(RUST_LIB_PATH "${RUST_MODULE_DIR}/target/release/${RUST_LIB_NAME}")
    endif()
else()
    set(RUST_TARGET "")
    set(RUST_LIB_NAME "libtcp_syn.a")
    set(RUST_LIB_PATH "${RUST_MODULE_DIR}/target/release/${RUST_LIB_NAME}")
endif()

if(RUST_TARGET)
    set(CARGO_BUILD_CMD cargo build --release --target ${RUST_TARGET})
else()
    set(CARGO_BUILD_CMD cargo build --release)
endif()

add_custom_command(
    OUTPUT "${RUST_LIB_PATH}"
    COMMAND ${CARGO_BUILD_CMD}
    WORKING_DIRECTORY "${RUST_MODULE_DIR}"
    COMMENT "Building Rust TCP SYN module with cargo..."
    VERBATIM
)

add_custom_target(rust_tcp_syn DEPENDS "${RUST_LIB_PATH}")

# ── Main executable ──────────────────────────────────────────
add_executable(dos_tool ${C_SOURCES} ${CXX_SOURCES})

# Wait for Rust build before linking
add_dependencies(dos_tool rust_tcp_syn)

# Include directories (project root so headers resolve)
target_include_directories(dos_tool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link Rust static library
target_link_libraries(dos_tool PRIVATE "${RUST_LIB_PATH}")

# ── Platform-specific link libraries ─────────────────────────
if(WIN32)
    target_link_libraries(dos_tool PRIVATE
        ws2_32       # Winsock
        userenv      # Rust stdlib
        bcrypt       # Rust stdlib
        ntdll        # Rust stdlib
        advapi32     # Rust stdlib
    )
    # MSVC-specific flags
    if(MSVC)
        target_compile_options(dos_tool PRIVATE /W3)
        # Silence deprecation warnings for POSIX functions
        target_compile_definitions(dos_tool PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _WINSOCK_DEPRECATED_NO_WARNINGS
        )
    endif()
else()
    target_link_libraries(dos_tool PRIVATE
        pthread
        dl
        m
    )
endif()

# ── Output ────────────────────────────────────────────────────
set_target_properties(dos_tool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "DoS Tool - C/C++/Rust Combined Build")
message(STATUS "  C standard:   ${CMAKE_C_STANDARD}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Rust lib:     ${RUST_LIB_PATH}")
