# FindSTM32CubeC0.cmake
#
# Поиск SDK STM32CubeC0. SDK лежит в соседней директории
# (../STM32CubeC0, ../../STM32CubeC0), либо путь задаётся:
#   - переменной CMake: -DSTM32CUBE_C0_PATH=/путь/к/STM32CubeC0
#   - переменной окружения: STM32CUBE_C0_PATH
#
# Проект использует только заголовки CMSIS (Core + Device) и
# заголовки LL/HAL; исходники HAL не компилируются. Поэтому модуль
# создаёт INTERFACE-библиотеку только с include-путями.

include(FindPackageHandleStandardArgs)

set(_STM32CUBE_C0_HINTS
    ${STM32CUBE_C0_PATH}
    $ENV{STM32CUBE_C0_PATH}
    ${CMAKE_CURRENT_LIST_DIR}/../STM32CubeC0
    ${CMAKE_CURRENT_LIST_DIR}/../../STM32CubeC0
)

find_path(STM32CubeC0_CMSIS_CORE_INCLUDE_DIR
    NAMES core_cm0plus.h
    HINTS ${_STM32CUBE_C0_HINTS}
    PATH_SUFFIXES Drivers/CMSIS/Core/Include
)

find_path(STM32CubeC0_CMSIS_DEVICE_INCLUDE_DIR
    NAMES stm32c031xx.h
    HINTS ${_STM32CUBE_C0_HINTS}
    PATH_SUFFIXES Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

find_path(STM32CubeC0_HAL_INCLUDE_DIR
    NAMES stm32c0xx_ll_gpio.h
    HINTS ${_STM32CUBE_C0_HINTS}
    PATH_SUFFIXES Drivers/STM32C0xx_HAL_Driver/Inc
)

find_package_handle_standard_args(STM32CubeC0
    REQUIRED_VARS
        STM32CubeC0_CMSIS_CORE_INCLUDE_DIR
        STM32CubeC0_CMSIS_DEVICE_INCLUDE_DIR
        STM32CubeC0_HAL_INCLUDE_DIR
)

if(STM32CubeC0_FOUND AND NOT TARGET STM32CubeC0::STM32CubeC0)
    add_library(STM32CubeC0::STM32CubeC0 INTERFACE IMPORTED)
    set_target_properties(STM32CubeC0::STM32CubeC0 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${STM32CubeC0_CMSIS_CORE_INCLUDE_DIR};${STM32CubeC0_CMSIS_DEVICE_INCLUDE_DIR};${STM32CubeC0_HAL_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(
    STM32CubeC0_CMSIS_CORE_INCLUDE_DIR
    STM32CubeC0_CMSIS_DEVICE_INCLUDE_DIR
    STM32CubeC0_HAL_INCLUDE_DIR
)