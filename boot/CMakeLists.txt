# boot/CMakeLists.txt
#
# Образ загрузчика: 8 KB @ 0x08000000 (stm32f103_boot.ld).

add_executable(boot.elf
    src/main.c
    src/boot.c
    src/request.c
)

stm32f103_configure(boot.elf)
target_compile_definitions(boot.elf PRIVATE BOOT_BUILD)

target_include_directories(boot.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(boot.elf PRIVATE
    platform_stm32f103
    hal_boot
    common
)

target_link_options(boot.elf PRIVATE
    ${STM32F103_LINK_FLAGS}
    -T ${BOOT_LDSCRIPT}
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

add_custom_target(flash-boot
    COMMAND ${OPENOCD_EXECUTABLE}
        -f ${OPENOCD_SCRIPTS}/interface/stlink.cfg
        -f ${OPENOCD_SCRIPTS}/target/stm32f1x.cfg
        -c "program $<TARGET_FILE:boot.elf> verify reset exit"
    DEPENDS boot.elf
    COMMENT "Прошивка загрузчика через ST-Link v2 (OpenOCD)"
    USES_TERMINAL
)

add_custom_target(erase-boot
    COMMAND ${OPENOCD_EXECUTABLE}
        -f ${OPENOCD_SCRIPTS}/interface/stlink.cfg
        -f ${OPENOCD_SCRIPTS}/target/stm32f1x.cfg
        -c "init" -c "halt" -c "stm32f1x mass_erase 0" -c "exit"
    COMMENT "Полное стирание Flash (загрузчик + приложение + метаданные)"
    USES_TERMINAL
)
