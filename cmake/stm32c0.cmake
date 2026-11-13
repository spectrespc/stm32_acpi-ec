# stm32c0.cmake
#
# Общие настройки для STM32C011x6 / STM32C031x6 (Cortex-M0+).
# Подключается из корневого CMakeLists.txt через include(stm32c0);
# EC_MCU_DEFINE и EC_RAM_SIZE_KB выбираются по MCU.

set(STM32C031_CPU_FLAGS -mcpu=cortex-m0plus -mthumb)

set(STM32C031_C_FLAGS
    ${STM32C031_CPU_FLAGS}
    -Os
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -std=c11
)

set(STM32C031_ASM_FLAGS
    ${STM32C031_CPU_FLAGS}
    -x assembler-with-cpp
)

set(STM32C031_LINK_FLAGS
    ${STM32C031_CPU_FLAGS}
    -Wl,--gc-sections
    -Wl,--nmagic
    --specs=nano.specs
    --specs=nosys.specs
    -nostartfiles
)

# Значения, которые корневой CMakeLists.txt передаёт в подкаталоги.
# EC_MCU_DEFINE/EC_RAM_SIZE_KB выбираются по MCU (C011xx или C031xx).
if(NOT DEFINED EC_MCU_DEFINE)
    set(EC_MCU_DEFINE STM32C031xx)
endif()
if(NOT DEFINED EC_RAM_SIZE_KB)
    set(EC_RAM_SIZE_KB 12)
endif()
set(EC_C_FLAGS ${STM32C031_C_FLAGS})
set(EC_ASM_FLAGS ${STM32C031_ASM_FLAGS})
set(EC_LINK_FLAGS ${STM32C031_LINK_FLAGS})
set(EC_HAL_DIR hal/c0)
set(EC_PLATFORM_DIR platform/stm32c0)
set(EC_SDK_TARGET STM32CubeC0::STM32CubeC0)

# Применяет общие настройки MCU к цели.
function(ec_configure target)
    target_compile_options(${target} PRIVATE ${EC_C_FLAGS})
    target_compile_definitions(${target} PRIVATE
        ${EC_MCU_DEFINE}
        USE_FULL_LL_DRIVER
        FLASH_TOTAL_KB=${FLASH_TOTAL_KB}
        EC_RAM_SIZE_KB=${EC_RAM_SIZE_KB}
    )
    target_link_libraries(${target} PRIVATE ${EC_SDK_TARGET})
endfunction()