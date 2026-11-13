# hal/CMakeLists.txt
#
# HAL содержит различающийся между образами код (по BOOT_BUILD),
# поэтому компилируется дважды: hal_boot и hal_app.
#
# Семейство MCU выбирает каталог исходников (EC_HAL_DIR = hal/f1 или
# hal/c0) и общий каталог заголовков hal/include.

set(HAL_SOURCES
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}/board.c
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}/flash.c
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}/i2c.c
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}/power.c
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}/system.c
)

if(MCU MATCHES "^C0")
    list(APPEND HAL_SOURCES ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}/clock.c)
endif()

add_library(hal_boot OBJECT ${HAL_SOURCES})
ec_configure(hal_boot)
target_compile_definitions(hal_boot PRIVATE BOOT_BUILD)
target_include_directories(hal_boot PUBLIC
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}
)

add_library(hal_app OBJECT ${HAL_SOURCES})
ec_configure(hal_app)
target_compile_definitions(hal_app PRIVATE APP_BUILD ${EC_DEBUG_DEFINE})
target_include_directories(hal_app PUBLIC
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}
)
