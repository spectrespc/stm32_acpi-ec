# platform/stm32c0/CMakeLists.txt
#
# Платформенно-зависимый код STM32C031x6:
#   - startup_stm32c031xx.s : таблица векторов (CubeC0) и Reset_Handler
#   - stm32c031_boot.ld     : карта памяти загрузчика (8 KB @ 0x08000000)
#   - stm32c031_app.ld      : карта памяти приложения (20 KB @ 0x08002000)

add_library(platform OBJECT
    startup_stm32c031xx.s
)

set_source_files_properties(startup_stm32c031xx.s PROPERTIES LANGUAGE ASM)
target_compile_options(platform PRIVATE ${EC_ASM_FLAGS})
target_compile_definitions(platform PRIVATE ${EC_MCU_DEFINE})