# build_rust.cmake - CMake script to build the Rust TCP SYN module via Cargo

set(RUST_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rust_modules")

# Determine library name by platform
if(WIN32)
    set(RUST_LIB_NAME "tcp_syn.lib")
else()
    set(RUST_LIB_NAME "libtcp_syn.a")
endif()

set(RUST_LIB_PATH "${RUST_MODULE_DIR}/target/release/${RUST_LIB_NAME}")

# Custom command to build the Rust static library
add_custom_command(
    OUTPUT "${RUST_LIB_PATH}"
    COMMAND cargo build --release
    WORKING_DIRECTORY "${RUST_MODULE_DIR}"
    COMMENT "Building Rust TCP SYN module with cargo..."
    VERBATIM
)

# Custom target so other targets can depend on it
add_custom_target(rust_tcp_syn ALL DEPENDS "${RUST_LIB_PATH}")
