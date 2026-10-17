# FindSTM32CubeF1.cmake
#
# Поиск SDK STM32CubeF1 (v1.8.7). SDK лежит в соседней директории
# (../STM32CubeF1), либо путь задаётся:
#   - переменной CMake: -DSTM32CUBE_F1_PATH=/путь/к/STM32CubeF1
#   - переменной окружения: STM32CUBE_F1_PATH
#
# Проект использует только заголовки CMSIS (Core + Device) и
# заголовки LL/HAL; исходники HAL не компилируются. Поэтому модуль
# создаёт INTERFACE-библиотеку только с include-путями.

include(FindPackageHandleStandardArgs)

set(_STM32CUBE_F1_HINTS
    ${STM32CUBE_F1_PATH}
    $ENV{STM32CUBE_F1_PATH}
    ${CMAKE_CURRENT_LIST_DIR}/../STM32CubeF1
    ${CMAKE_CURRENT_LIST_DIR}/../../STM32CubeF1
)

find_path(STM32CubeF1_CMSIS_CORE_INCLUDE_DIR
    NAMES core_cm3.h
    HINTS ${_STM32CUBE_F1_HINTS}
    PATH_SUFFIXES Drivers/CMSIS/Include
)

find_path(STM32CubeF1_CMSIS_DEVICE_INCLUDE_DIR
    NAMES stm32f1xx.h
    HINTS ${_STM32CUBE_F1_HINTS}
    PATH_SUFFIXES Drivers/CMSIS/Device/ST/STM32F1xx/Include
)

find_path(STM32CubeF1_HAL_INCLUDE_DIR
    NAMES stm32f1xx_ll_gpio.h
    HINTS ${_STM32CUBE_F1_HINTS}
    PATH_SUFFIXES Drivers/STM32F1xx_HAL_Driver/Inc
)

find_package_handle_standard_args(STM32CubeF1
    REQUIRED_VARS
        STM32CubeF1_CMSIS_CORE_INCLUDE_DIR
        STM32CubeF1_CMSIS_DEVICE_INCLUDE_DIR
        STM32CubeF1_HAL_INCLUDE_DIR
)

if(STM32CubeF1_FOUND AND NOT TARGET STM32CubeF1::STM32CubeF1)
    add_library(STM32CubeF1::STM32CubeF1 INTERFACE IMPORTED)
    set_target_properties(STM32CubeF1::STM32CubeF1 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
            "${STM32CubeF1_CMSIS_CORE_INCLUDE_DIR};${STM32CubeF1_CMSIS_DEVICE_INCLUDE_DIR};${STM32CubeF1_HAL_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(
    STM32CubeF1_CMSIS_CORE_INCLUDE_DIR
    STM32CubeF1_CMSIS_DEVICE_INCLUDE_DIR
    STM32CubeF1_HAL_INCLUDE_DIR
)