# stm32f103.cmake
#
# Общие настройки для целей STM32F103 (Cortex-M3).
# Подключается из корневого CMakeLists.txt через include(stm32f103).

set(STM32F103_CPU_FLAGS -mcpu=cortex-m3 -mthumb)

# Флаги компиляции, идентичные исходным Makefile-проектам.
set(STM32F103_C_FLAGS
    ${STM32F103_CPU_FLAGS}
    -Os
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -std=c11
)

set(STM32F103_ASM_FLAGS
    ${STM32F103_CPU_FLAGS}
    -x assembler-with-cpp
)

# Общие флаги линковки для прошивок (тот же набор, что в Makefile).
set(STM32F103_LINK_FLAGS
    ${STM32F103_CPU_FLAGS}
    -Wl,--gc-sections
    --specs=nano.specs
    --specs=nosys.specs
    -nostartfiles
)

# Применяет общие настройки MCU к цели.
function(stm32f103_configure target)
    target_compile_options(${target} PRIVATE ${STM32F103_C_FLAGS})
    target_compile_definitions(${target} PRIVATE
        STM32F103xB
        USE_FULL_LL_DRIVER
        FLASH_TOTAL_KB=${FLASH_TOTAL_KB}
    )
    target_link_libraries(${target} PRIVATE STM32CubeF1::STM32CubeF1)
endfunction()