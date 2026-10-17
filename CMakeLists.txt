# CMakeLists.txt — точка входа проекта stm32_acpi_ec
#
# Кросс-сборка прошивок (bootloader + application):
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake [-DMCU=C8T6|CBT6]
#   cmake --build build
#
# Нативная сборка хостовых утилит и тестов:
#   cmake -B build_host
#   cmake --build build_host --target test

cmake_minimum_required(VERSION 3.20)

project(stm32_acpi_ec LANGUAGES C ASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(stm32f103)

# ============================================================================
# Выбор микроконтроллера
# ============================================================================

set(MCU "C8T6" CACHE STRING "Вариант MCU: C8T6 (64 KB) или CBT6 (128 KB)")
set_property(CACHE MCU PROPERTY STRINGS C8T6 CBT6)

if(MCU STREQUAL "C8T6")
    set(FLASH_TOTAL_KB 64)
    set(APP_FLASH_LENGTH 54K)
elseif(MCU STREQUAL "CBT6")
    set(FLASH_TOTAL_KB 128)
    set(APP_FLASH_LENGTH 120K)
else()
    message(FATAL_ERROR "Неизвестный MCU '${MCU}'. Используйте C8T6 или CBT6.")
endif()

set(BOOT_FLASH_LENGTH 8K)
set(APP_FLASH_ORIGIN 0x08002000)

# ============================================================================
# Версия прошивки из VCS (git describe) с запасным значением 0.1.0
# ============================================================================

set(FW_VERSION_STRING "1.0.0")

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

# ============================================================================
# Кросс-сборка: прошивки. Нативная: tools + tests.
# ============================================================================

if(CMAKE_CROSSCOMPILING)
    find_package(STM32CubeF1 REQUIRED)

    # Внешние инструменты и параметры прошивки
    set(OPENOCD_EXECUTABLE "openocd" CACHE FILEPATH "Исполняемый файл OpenOCD")
    set(OPENOCD_SCRIPTS "/usr/share/openocd/scripts" CACHE PATH "Каталог скриптов OpenOCD")
    set(I2C_DEVICE "/dev/i2c-7" CACHE STRING "I2C-устройство хоста для bootloader_tool")
    set(BOOTLOADER_TOOL "bootloader_tool" CACHE FILEPATH "Путь к собранному bootloader_tool")

    set(BOOT_LDSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f103/stm32f103_boot.ld")
    set(APP_LDSCRIPT  "${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f103/stm32f103_app.ld")

    add_subdirectory(platform/stm32f103)
    add_subdirectory(hal)
    add_subdirectory(common)
    add_subdirectory(boot)
    add_subdirectory(app)
else()
    message(STATUS "Нативная сборка: только tools/ и tests/")
    add_subdirectory(tools)
    enable_testing()
    add_subdirectory(tests)
endif()