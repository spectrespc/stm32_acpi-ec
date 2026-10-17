# tools/bootloader_tool/CMakeLists.txt

add_executable(bootloader_tool
    main.c
    crc_host.c
    i2c_transport.c
)

target_compile_options(bootloader_tool PRIVATE
    -Wall
    -Wextra
    -O2
    -std=c99
)

target_include_directories(bootloader_tool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})