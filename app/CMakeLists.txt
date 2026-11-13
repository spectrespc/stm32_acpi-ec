# app/CMakeLists.txt
#
# Образ приложения: 54/120/20 KB @ 0x08002000 (stm32f103_app.ld или
# stm32c031_app.ld — выбирается MCU).

add_executable(app.elf
    src/main.c
    src/battery.c
    src/bus_watchdog.c
    src/commands.c
    src/power_task.c
)

ec_configure(app.elf)
target_compile_definitions(app.elf PRIVATE APP_BUILD ${EC_DEBUG_DEFINE})

target_include_directories(app.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(app.elf PRIVATE
    platform
    hal_app
    common
)

target_link_options(app.elf PRIVATE
    ${EC_LINK_FLAGS}
    -T ${APP_LDSCRIPT}
    ${EC_RAM_LINK_DEFS}
    -Wl,--defsym=FLASH_ORIGIN=${APP_FLASH_ORIGIN}
    -Wl,--defsym=FLASH_LENGTH=${APP_FLASH_LENGTH}
    -Wl,-Map=${CMAKE_BINARY_DIR}/app.map
)

# .elf кладётся в корень сборки с расширением .elf — как ожидают
# README, CI и release-скрипты (build/app.elf).
set_target_properties(app.elf PROPERTIES
    OUTPUT_NAME app
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LINK_DEPENDS ${APP_LDSCRIPT}
)

add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:app.elf> ${CMAKE_BINARY_DIR}/app.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app.elf> ${CMAKE_BINARY_DIR}/app.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/app.bin
            ${CMAKE_BINARY_DIR}/app-${FW_VERSION_STRING}.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/app.hex
            ${CMAKE_BINARY_DIR}/app-${FW_VERSION_STRING}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:app.elf>
    COMMENT "Формирование app.hex / app.bin и app-${FW_VERSION_STRING}.bin/.hex"
)

# ============================================================================
# Прошивка
# ============================================================================

if(MCU MATCHES "^C0")
    set(FLASH_OPENOCD_ARGS -f ${CMAKE_SOURCE_DIR}/scripts/openocd_c0.cfg)
else()
    set(FLASH_OPENOCD_ARGS
        -f ${OPENOCD_SCRIPTS}/interface/stlink.cfg
        -f ${OPENOCD_SCRIPTS}/target/stm32f1x.cfg)
endif()

add_custom_target(flash-app
    COMMAND ${OPENOCD_EXECUTABLE} ${FLASH_OPENOCD_ARGS}
        -c "program $<TARGET_FILE:app.elf> verify reset exit"
    DEPENDS app.elf
    COMMENT "Прошивка приложения через ST-Link v2 (OpenOCD)"
    USES_TERMINAL
)

# Прошивка через I2C-загрузчик (требует bootloader_tool на хосте).
add_custom_target(flash-app-i2c
    COMMAND ${BOOTLOADER_TOOL} ec ${I2C_DEVICE} flash
            ${CMAKE_BINARY_DIR}/app.bin ${FW_VERSION_ARG}
    COMMAND ${BOOTLOADER_TOOL} ec ${I2C_DEVICE} reset
    DEPENDS app.elf
    COMMENT "Прошивка приложения через bootloader_tool (I2C ${I2C_DEVICE})"
    USES_TERMINAL
)

add_custom_target(info-app
    COMMAND ${BOOTLOADER_TOOL} ec ${I2C_DEVICE} info
    COMMENT "GET_INFO через bootloader_tool"
    USES_TERMINAL
)
