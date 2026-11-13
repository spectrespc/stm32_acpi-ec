# platform/stm32f103/CMakeLists.txt
#
# Платформенно-зависимый код, единый для загрузчика и приложения:
#   - startup_stm32f103xb.s : таблица векторов и Reset_Handler
#   - stm32f103_boot.ld     : карта памяти загрузчика (8 KB @ 0x08000000)
#   - stm32f103_app.ld      : карта памяти приложения (54/120 KB @ 0x08002000)

add_library(platform OBJECT
    startup_stm32f103xb.s
)

set_source_files_properties(startup_stm32f103xb.s PROPERTIES LANGUAGE ASM)
target_compile_options(platform PRIVATE ${EC_ASM_FLAGS})
target_compile_definitions(platform PRIVATE ${EC_MCU_DEFINE})