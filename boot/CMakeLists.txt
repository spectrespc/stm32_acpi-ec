# boot/CMakeLists.txt
#
# Образ загрузчика: 8 KB @ 0x08000000 (stm32f103_boot.ld или
# stm32c031_boot.ld — выбирается MCU).

add_executable(boot.elf
    src/main.c
    src/boot.c
    src/request.c
)

ec_configure(boot.elf)
target_compile_definitions(boot.elf PRIVATE BOOT_BUILD)

# Диагностическая сборка: самопроверка Flash вместо запуска приложения.
option(BOOT_SELFTEST "Загрузчик: BOOT_SELFTEST — проверить erase/program/read" OFF)
if(BOOT_SELFTEST)
    target_compile_definitions(boot.elf PRIVATE BOOT_SELFTEST)
endif()

target_include_directories(boot.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(boot.elf PRIVATE
    platform
    hal_boot
    common
)

target_link_options(boot.elf PRIVATE
    ${EC_LINK_FLAGS}
    -T ${BOOT_LDSCRIPT}
    ${EC_RAM_LINK_DEFS}
    -Wl,--defsym=FLASH_LEN=${BOOT_FLASH_LENGTH}
    -Wl,-Map=${CMAKE_BINARY_DIR}/bootloader.map
)

# .elf кладётся в корень сборки с расширением .elf — как ожидают
# README, CI и release-скрипты (build/bootloader.elf).
set_target_properties(boot.elf PROPERTIES
    OUTPUT_NAME bootloader
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LINK_DEPENDS ${BOOT_LDSCRIPT}
)

add_custom_command(TARGET boot.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:boot.elf> ${CMAKE_BINARY_DIR}/bootloader.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:boot.elf> ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:boot.elf>
    COMMENT "Формирование bootloader.hex / bootloader.bin"
)

# ============================================================================
# Прошивка
# ============================================================================

if(MCU STREQUAL "C031x6")
    set(FLASH_OPENOCD_ARGS -f ${CMAKE_SOURCE_DIR}/scripts/openocd_c0.cfg)
    set(ERASE_OPENOCD_COMMAND "stm32l4x mass_erase 0")
else()
    set(FLASH_OPENOCD_ARGS
        -f ${OPENOCD_SCRIPTS}/interface/stlink.cfg
        -f ${OPENOCD_SCRIPTS}/target/stm32f1x.cfg)
    set(ERASE_OPENOCD_COMMAND "stm32f1x mass_erase 0")
endif()

add_custom_target(flash-boot
    COMMAND ${OPENOCD_EXECUTABLE} ${FLASH_OPENOCD_ARGS}
        -c "program $<TARGET_FILE:boot.elf> verify reset exit"
    DEPENDS boot.elf
    COMMENT "Прошивка загрузчика через ST-Link v2 (OpenOCD)"
    USES_TERMINAL
)

add_custom_target(erase-boot
    COMMAND ${OPENOCD_EXECUTABLE} ${FLASH_OPENOCD_ARGS}
        -c "init" -c "halt" -c "${ERASE_OPENOCD_COMMAND}" -c "exit"
    COMMENT "Полное стирание Flash (загрузчик + приложение + метаданные)"
    USES_TERMINAL
)