# arm-none-eabi.cmake
#
# Toolchain-файл кросс-компиляции для STM32F103 (Cortex-M3).
# Проверен на arm-none-eabi-gcc 12.2.1.
#
# Использование:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "")
set(CMAKE_SIZE    arm-none-eabi-size    CACHE FILEPATH "")

# Пробная компиляция не должна пытаться линковать исполняемый файл
# под target: при кросс-сборке используем статическую библиотеку.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# SDK ищется в дереве исходников, а не в sysroot toolchain-а.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
