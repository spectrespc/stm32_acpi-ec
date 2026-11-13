# CMakeLists.txt — точка входа проекта stm32_acpi_ec
#
# Кросс-сборка прошивок (bootloader + application):
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake [-DMCU=C8T6|CBT6|C011x6|C031x6]
#   cmake --build build
#
# Нативная сборка хостовых утилит и тестов:
#   cmake -B build_host
#   cmake --build build_host --target test

cmake_minimum_required(VERSION 3.20)

project(stm32_acpi_ec LANGUAGES C ASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ============================================================================
# Выбор микроконтроллера
# ============================================================================

set(MCU "C8T6" CACHE STRING "Вариант MCU: C8T6 (64 KB), CBT6 (128 KB), C011x6 (32 KB/6 KB) или C031x6 (32 KB/12 KB)")
set_property(CACHE MCU PROPERTY STRINGS C8T6 CBT6 C011x6 C031x6)

# Отладочный слой I2C (счётчики, 0x28/0x29). Выключен в проде: команды
# отвечают 0xFF, лишние счётчики не занимают RAM.
option(EC_DEBUG_BUILD "Включить отладочные команды I2C 0x28/0x29 и счётчики" OFF)
if(EC_DEBUG_BUILD)
    set(EC_DEBUG_DEFINE DEBUG_BUILD)
endif()

if(MCU STREQUAL "C8T6")
    set(FLASH_TOTAL_KB 64)
    set(APP_FLASH_LENGTH 54K)
    include(stm32f103)
elseif(MCU STREQUAL "CBT6")
    set(FLASH_TOTAL_KB 128)
    set(APP_FLASH_LENGTH 120K)
    include(stm32f103)
elseif(MCU STREQUAL "C031x6")
    set(FLASH_TOTAL_KB 32)
    set(APP_FLASH_LENGTH 20K)
    set(EC_MCU_DEFINE STM32C031xx)
    set(EC_RAM_SIZE_KB 12)
    set(EC_RAM_LENGTH 12272)
    set(EC_FLAG_ADDR 0x20002FFC)
    include(stm32c0)
elseif(MCU STREQUAL "C011x6")
    set(FLASH_TOTAL_KB 32)
    set(APP_FLASH_LENGTH 20K)
    set(EC_MCU_DEFINE STM32C011xx)
    set(EC_RAM_SIZE_KB 6)
    set(EC_RAM_LENGTH 6128)
    set(EC_FLAG_ADDR 0x200017FC)
    include(stm32c0)
else()
    message(FATAL_ERROR "Неизвестный MCU '${MCU}'. Используйте C8T6, CBT6, C011x6 или C031x6.")
endif()

set(BOOT_FLASH_LENGTH 8K)
set(APP_FLASH_ORIGIN 0x08002000)

set(BOOT_LDSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f103/stm32f103_boot.ld")
set(APP_LDSCRIPT  "${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f103/stm32f103_app.ld")

if(MCU MATCHES "^C0")
    set(BOOT_LDSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32c0/stm32c0_boot.ld")
    set(APP_LDSCRIPT  "${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32c0/stm32c0_app.ld")
    set(EC_RAM_LINK_DEFS
        -Wl,--defsym=RAM_LENGTH=${EC_RAM_LENGTH}
        -Wl,--defsym=ECFLAG_ADDR=${EC_FLAG_ADDR})
else()
    set(EC_RAM_LINK_DEFS "")
endif()

# ============================================================================
# Версия прошивки из VCS (git describe) с запасным значением 1.0.0
# ============================================================================

set(FW_VERSION_STRING "1.0.0" CACHE STRING "Firmware version (major.minor.patch)")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_rc
    )
    if(_git_rc EQUAL 0 AND _git_describe MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(FW_VERSION_STRING "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
    endif()
endif()

if(FW_VERSION_STRING MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(_ver_major ${CMAKE_MATCH_1})
    set(_ver_minor ${CMAKE_MATCH_2})
    set(_ver_patch ${CMAKE_MATCH_3})
else()
    set(_ver_major 1)
    set(_ver_minor 0)
    set(_ver_patch 0)
endif()

math(EXPR FW_VERSION_HEX
    "${_ver_major}*65536 + ${_ver_minor}*256 + ${_ver_patch}"
    OUTPUT_FORMAT HEXADECIMAL)
string(REGEX REPLACE "^0x" "" FW_VERSION_HEX "${FW_VERSION_HEX}")

# Аргумент версии для bootloader_tool (формат как в старом Makefile)
set(FW_VERSION_ARG "0x${FW_VERSION_HEX}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/common/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/version.h"
    @ONLY)

include_directories("${CMAKE_BINARY_DIR}/generated")

message(STATUS "MCU: ${MCU} (FLASH_TOTAL_KB=${FLASH_TOTAL_KB}, app ${APP_FLASH_LENGTH})")
message(STATUS "FW_VERSION: ${FW_VERSION_STRING} (0x${FW_VERSION_HEX})")
message(STATUS "I2C debug layer: ${EC_DEBUG_BUILD}")

# ============================================================================
# Кросс-сборка: прошивки. Нативная: tools + tests.
# ============================================================================

if(CMAKE_CROSSCOMPILING)
    if(MCU MATCHES "^C0")
        find_package(STM32CubeC0 REQUIRED)
    else()
        find_package(STM32CubeF1 REQUIRED)
    endif()

    # Внешние инструменты и параметры прошивки
    set(OPENOCD_EXECUTABLE "openocd" CACHE FILEPATH "Исполняемый файл OpenOCD")
    set(OPENOCD_SCRIPTS "/usr/share/openocd/scripts" CACHE PATH "Каталог скриптов OpenOCD")

    # Параметры прошивки: берутся из окружения (env) или -D, иначе значение
    # по умолчанию. Env-переменные используются CI (flash.yml) и всегда
    # переопределяют кэш (FORCE), чтобы повторная конфигурация не сохраняла
    # устаревший путь. Пустая env-переменная игнорируется (fallthrough).
    if(DEFINED ENV{BOOTLOADER_TOOL} AND NOT "$ENV{BOOTLOADER_TOOL}" STREQUAL "")
        set(BOOTLOADER_TOOL "$ENV{BOOTLOADER_TOOL}" CACHE FILEPATH "Путь к собранному bootloader_tool" FORCE)
    elseif(NOT DEFINED BOOTLOADER_TOOL)
        set(BOOTLOADER_TOOL "bootloader_tool" CACHE FILEPATH "Путь к собранному bootloader_tool")
    endif()

    if(DEFINED ENV{I2C_DEVICE} AND NOT "$ENV{I2C_DEVICE}" STREQUAL "")
        set(I2C_DEVICE "$ENV{I2C_DEVICE}" CACHE STRING "I2C-устройство хоста для bootloader_tool" FORCE)
    elseif(NOT DEFINED I2C_DEVICE)
        set(I2C_DEVICE "/dev/i2c-7" CACHE STRING "I2C-устройство хоста для bootloader_tool")
    endif()


    add_subdirectory(${EC_PLATFORM_DIR})
    add_subdirectory(hal)
    add_subdirectory(common)
    add_subdirectory(boot)
    add_subdirectory(app)
else()
    message(STATUS "Нативная сборка: только tools/ и tests/")
    add_subdirectory(tools)
    enable_testing()
    add_subdirectory(tests)


    # ========================================================================
    # Код-стайл и статический анализ (только нативная сборка).
    # Цели не создаются, если нужный инструмент не найден в PATH.
    # ========================================================================


    find_program(CLANG_FORMAT_EXECUTABLE clang-format)
    if(CLANG_FORMAT_EXECUTABLE)
        file(GLOB_RECURSE ALL_SOURCE_FILES
            ${CMAKE_SOURCE_DIR}/boot/src/*.c   ${CMAKE_SOURCE_DIR}/boot/inc/*.h
            ${CMAKE_SOURCE_DIR}/app/src/*.c    ${CMAKE_SOURCE_DIR}/app/inc/*.h
            ${CMAKE_SOURCE_DIR}/hal/include/*.c ${CMAKE_SOURCE_DIR}/hal/include/*.h
            ${CMAKE_SOURCE_DIR}/hal/f1/*.c      ${CMAKE_SOURCE_DIR}/hal/f1/*.h
            ${CMAKE_SOURCE_DIR}/hal/c0/*.c      ${CMAKE_SOURCE_DIR}/hal/c0/*.h
            ${CMAKE_SOURCE_DIR}/common/*.c     ${CMAKE_SOURCE_DIR}/common/*.h
            ${CMAKE_SOURCE_DIR}/tools/*.c      ${CMAKE_SOURCE_DIR}/tools/*.h
            ${CMAKE_SOURCE_DIR}/tests/*.c      ${CMAKE_SOURCE_DIR}/tests/*.h
        )
        add_custom_target(format-check
            COMMAND ${CLANG_FORMAT_EXECUTABLE}
                --style=file --dry-run --Werror ${ALL_SOURCE_FILES}
            COMMENT "Проверка форматирования (clang-format)"
        )
        add_custom_target(format
            COMMAND ${CLANG_FORMAT_EXECUTABLE}
                --style=file -i ${ALL_SOURCE_FILES}
            COMMENT "Автоматическое форматирование кода"
        )
    endif()

    find_program(CPPCHECK_EXECUTABLE cppcheck)
    if(CPPCHECK_EXECUTABLE)
        # Различающийся код включается по BOOT_BUILD, поэтому анализ идёт
        # двумя проходами: загрузчик (-DBOOT_BUILD) и приложение (без него).
        # Только так проверяются обе ветки hal/ (#ifdef/#else).
        # FLASH_TOTAL_KB берётся из выбранного MCU (-DMCU=C8T6|CBT6).
        set(CPPCHECK_ARGS
            --enable=warning,style,performance,portability
            --error-exitcode=1
            --inline-suppr
            --suppress=unusedFunction
            --suppress=normalCheckLevelMaxBranches
            --suppress=missingIncludeSystem
            --std=c11
            --platform=unix32
            -I ${CMAKE_SOURCE_DIR}/common
            -I ${CMAKE_SOURCE_DIR}/hal/include
            -I ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}
            -I ${CMAKE_SOURCE_DIR}/boot/inc
            -I ${CMAKE_SOURCE_DIR}/app/inc
            -D${EC_MCU_DEFINE} -DFLASH_TOTAL_KB=${FLASH_TOTAL_KB}
        )
        add_custom_target(cppcheck
            COMMAND ${CPPCHECK_EXECUTABLE} ${CPPCHECK_ARGS} -DBOOT_BUILD
                ${CMAKE_SOURCE_DIR}/boot/src
                ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}
                ${CMAKE_SOURCE_DIR}/common
            COMMAND ${CPPCHECK_EXECUTABLE} ${CPPCHECK_ARGS}
                ${CMAKE_SOURCE_DIR}/app/src
                ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}
                ${CMAKE_SOURCE_DIR}/common
            COMMENT "Статический анализ (cppcheck: boot + app)"
            USES_TERMINAL
        )
    endif()
endif()
