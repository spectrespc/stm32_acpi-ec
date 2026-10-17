# hal/CMakeLists.txt
#
# HAL содержит различающийся между образами код (по BOOT_BUILD),
# поэтому компилируется дважды: hal_boot и hal_app.

set(HAL_SOURCES
    board.c
    flash.c
    i2c.c
    system.c
)

add_library(hal_boot OBJECT ${HAL_SOURCES})
stm32f103_configure(hal_boot)
target_compile_definitions(hal_boot PRIVATE BOOT_BUILD)
target_include_directories(hal_boot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(hal_app OBJECT ${HAL_SOURCES})
stm32f103_configure(hal_app)
target_compile_definitions(hal_app PRIVATE APP_BUILD)
target_include_directories(hal_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})